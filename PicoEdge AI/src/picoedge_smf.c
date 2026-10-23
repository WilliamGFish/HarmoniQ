#include "picoedge.h"

#include "picoedge_model.h"
#define PE_FLAG_JUST_PRIMED   (1U << 0)
#define PE_FLAG_FROM_FAULT    (1U << 1)
#define PE_FLAG_SILENT_RETURN (1U << 2)

enum pe_smf_state_id {
    PE_STATE_LEARNING = 0,
    PE_STATE_MONITOR_PARENT = 1,
    PE_STATE_NOMINAL = 2,
    PE_STATE_SUSPECT = 3,
    PE_STATE_FAULT = 4
};

#ifdef __ZEPHYR__
static const struct smf_state pe_smf_states[];

static void pe_emit_event(struct pe_context *ctx, pe_event_type_t evt) {
    ctx->seq_num++;
    pe_diag_frame_t frame;
    frame.magic = PE_MAGIC_HEADER;
    frame.node_id = ctx->node_id;
    frame.event_flags = ((evt == PE_EVT_FAULT_CONFIRMED) ? 0x01U : 0x00U) |
                        ((evt == PE_EVT_REGIME_SHIFT) ? 0x02U : 0x00U) |
                        ((ctx->ad.total_samples >= ctx->ad.min_samples) ? 0x04U : 0x00U);
    frame.similarity_pct = (uint8_t)(ctx->last_similarity_pct + 0.5f);
    frame.active_regime = ctx->active_regime;
    frame.predicted_class = ctx->predicted_class;
    frame.class_confidence = (uint8_t)(ctx->last_confidence_pct + 0.5f);
    frame.most_deviant_feat = ctx->most_deviant_feat;
    frame.anomaly_streak = ctx->anomaly_streak;
    frame.seq_num = ctx->seq_num;

    if (ctx->event_cb != NULL) {
        ctx->event_cb(evt, &frame, ctx->user_data);
    }
}

static void s_learning_run(void *obj) {
    struct pe_context *ctx = (struct pe_context *)obj;
    pe_core_ad_learn(&ctx->ad, ctx->current_feats, ctx->active_feats_mask);

    if (ctx->ad.total_samples >= ctx->ad.min_samples) {
        pe_core_ad_detect(&ctx->ad, ctx->current_feats, ctx->active_feats_mask, &ctx->last_similarity_pct, &ctx->active_regime, &ctx->most_deviant_feat);
        ctx->prev_regime = ctx->active_regime;
        ctx->op_mode = PE_MODE_MONITOR;
        ctx->flags |= PE_FLAG_JUST_PRIMED;
        smf_set_state(SMF_CTX(ctx), &pe_smf_states[PE_STATE_NOMINAL]);
    }
}



static void s_nominal_entry(void *obj) {
    struct pe_context *ctx = (struct pe_context *)obj;
    ctx->anomaly_streak = 0;
    ctx->recovery_streak = 0;

    if (ctx->flags & PE_FLAG_SILENT_RETURN) {
        ctx->flags &= ~PE_FLAG_SILENT_RETURN;
        return;
    } else if (ctx->flags & PE_FLAG_JUST_PRIMED) {
        ctx->flags &= ~PE_FLAG_JUST_PRIMED;
        pe_emit_event(ctx, PE_EVT_BASELINE_READY);
    } else if (ctx->flags & PE_FLAG_FROM_FAULT) {
        ctx->flags &= ~PE_FLAG_FROM_FAULT;
        pe_emit_event(ctx, PE_EVT_FAULT_CLEARED);
    }
}

static void s_nominal_run(void *obj) {
    struct pe_context *ctx = (struct pe_context *)obj;

    pe_core_ad_detect(&ctx->ad, ctx->current_feats, ctx->active_feats_mask, &ctx->last_similarity_pct, &ctx->active_regime, &ctx->most_deviant_feat);
    ctx->predicted_class = pe_core_clf_predict(&ctx->clf, ctx->current_feats, ctx->active_feats_mask, &ctx->last_confidence_pct);

    if (ctx->last_similarity_pct < (float)CONFIG_PE_ANOMALY_THRESH_PCT) {
        ctx->anomaly_streak = 1;
        if (CONFIG_PE_DEBOUNCE_WINDOWS <= 1) {
            smf_set_state(SMF_CTX(ctx), &pe_smf_states[PE_STATE_FAULT]);
        } else {
            smf_set_state(SMF_CTX(ctx), &pe_smf_states[PE_STATE_SUSPECT]);
        }
    } else if (ctx->active_regime != ctx->prev_regime) {
        ctx->prev_regime = ctx->active_regime;
        pe_emit_event(ctx, PE_EVT_REGIME_SHIFT);
    }
}

static void s_suspect_run(void *obj) {
    struct pe_context *ctx = (struct pe_context *)obj;

    pe_core_ad_detect(&ctx->ad, ctx->current_feats, ctx->active_feats_mask, &ctx->last_similarity_pct, &ctx->active_regime, &ctx->most_deviant_feat);
    ctx->predicted_class = pe_core_clf_predict(&ctx->clf, ctx->current_feats, ctx->active_feats_mask, &ctx->last_confidence_pct);

    if (ctx->last_similarity_pct < (float)CONFIG_PE_ANOMALY_THRESH_PCT) {
        ctx->anomaly_streak++;
        if (ctx->anomaly_streak >= CONFIG_PE_DEBOUNCE_WINDOWS) {
            smf_set_state(SMF_CTX(ctx), &pe_smf_states[PE_STATE_FAULT]);
        }
    } else {
        ctx->flags |= PE_FLAG_SILENT_RETURN;
        smf_set_state(SMF_CTX(ctx), &pe_smf_states[PE_STATE_NOMINAL]);
    }
}

static void s_fault_entry(void *obj) {
    struct pe_context *ctx = (struct pe_context *)obj;
    ctx->recovery_streak = 0;
    ctx->flags |= PE_FLAG_FROM_FAULT;
    pe_emit_event(ctx, PE_EVT_FAULT_CONFIRMED);
}

static void s_fault_run(void *obj) {
    struct pe_context *ctx = (struct pe_context *)obj;

    pe_core_ad_detect(&ctx->ad, ctx->current_feats, ctx->active_feats_mask, &ctx->last_similarity_pct, &ctx->active_regime, &ctx->most_deviant_feat);
    ctx->predicted_class = pe_core_clf_predict(&ctx->clf, ctx->current_feats, ctx->active_feats_mask, &ctx->last_confidence_pct);

    if (ctx->last_similarity_pct >= (float)CONFIG_PE_HYSTERESIS_CLEAR_PCT) {
        ctx->recovery_streak++;
        if (ctx->recovery_streak >= CONFIG_PE_DEBOUNCE_WINDOWS) {
            smf_set_state(SMF_CTX(ctx), &pe_smf_states[PE_STATE_NOMINAL]);
        }
    } else {
        if (ctx->anomaly_streak < 255) ctx->anomaly_streak++;
        ctx->recovery_streak = 0;
    }
}

static const struct smf_state pe_smf_states[] = {
    [PE_STATE_LEARNING] = SMF_CREATE_STATE(NULL, s_learning_run, NULL, NULL),
    [PE_STATE_MONITOR_PARENT] = SMF_CREATE_STATE(NULL, NULL, NULL, NULL),
    [PE_STATE_NOMINAL] = SMF_CREATE_STATE(s_nominal_entry, s_nominal_run, NULL, NULL),
    [PE_STATE_SUSPECT] = SMF_CREATE_STATE(NULL, s_suspect_run, NULL, NULL),
    [PE_STATE_FAULT] = SMF_CREATE_STATE(s_fault_entry, s_fault_run, NULL, NULL)
};
#endif

int pe_init(struct pe_context *ctx, uint8_t node_id, pe_event_cb_t cb, void *user_data) {
    if (!ctx) return -1;

    memset(ctx, 0, sizeof(*ctx));
    pe_core_ad_init(&ctx->ad, CONFIG_PE_MIN_LEARN_WINDOWS, 1.0f);
    pe_core_clf_init(&ctx->clf);

    ctx->event_cb = cb;
    ctx->user_data = user_data;
    ctx->node_id = node_id;
    ctx->active_feats_mask = PE_DEFAULT_ACTIVE_FEATS_MASK;
    ctx->op_mode = PE_MODE_MONITOR;

#ifdef __ZEPHYR__
    smf_set_initial(SMF_CTX(ctx), &pe_smf_states[PE_STATE_LEARNING]);
#else
    ctx->smf.state = PE_STATE_LEARNING;
#endif
    return 0;
}

int pe_set_mode(struct pe_context *ctx, pe_op_mode_t mode, uint8_t target_fault_class) {
    if (!ctx) return -1;

    if (mode == PE_MODE_LEARN_NORMAL) {
        ctx->op_mode = PE_MODE_LEARN_NORMAL;
#ifdef __ZEPHYR__
        smf_set_state(SMF_CTX(ctx), &pe_smf_states[PE_STATE_LEARNING]);
#else
        ctx->smf.state = PE_STATE_LEARNING;
#endif
    } else if (mode == PE_MODE_LEARN_FAULT_CLASS) {
        if (target_fault_class >= CONFIG_PE_MAX_CLASSES) return -1;
        ctx->op_mode = PE_MODE_LEARN_FAULT_CLASS;
        ctx->target_learn_class = target_fault_class;
    } else {
        ctx->op_mode = PE_MODE_MONITOR;
#ifdef __ZEPHYR__
        if (ctx->ad.total_samples >= ctx->ad.min_samples) {
            smf_set_state(SMF_CTX(ctx), &pe_smf_states[PE_STATE_NOMINAL]);
        } else {
            smf_set_state(SMF_CTX(ctx), &pe_smf_states[PE_STATE_LEARNING]);
        }
#else
        if (ctx->ad.total_samples >= ctx->ad.min_samples) {
            ctx->smf.state = PE_STATE_NOMINAL;
        } else {
            ctx->smf.state = PE_STATE_LEARNING;
        }
#endif
    }
    return 0;
}

int pe_process_window(struct pe_context *ctx, const float *raw_sensor_window) {
    if (!ctx || !raw_sensor_window) return -1;

    pe_extract_features_masked(raw_sensor_window, ctx->current_feats, ctx->active_feats_mask);

    if (ctx->op_mode == PE_MODE_LEARN_FAULT_CLASS) {
        pe_core_clf_learn(&ctx->clf, ctx->current_feats, ctx->target_learn_class);
        return 0;
    }

#ifdef __ZEPHYR__
    return smf_run_state(SMF_CTX(ctx));
#else
    // Stub implementation for tests outside zephyr
    if (ctx->smf.state == PE_STATE_LEARNING) {
        pe_core_ad_learn(&ctx->ad, ctx->current_feats, ctx->active_feats_mask);
        if (ctx->ad.total_samples >= ctx->ad.min_samples) {
            ctx->smf.state = PE_STATE_NOMINAL;
        }
    } else {
        pe_core_ad_detect(&ctx->ad, ctx->current_feats, ctx->active_feats_mask, &ctx->last_similarity_pct, &ctx->active_regime, &ctx->most_deviant_feat);
        ctx->predicted_class = pe_core_clf_predict(&ctx->clf, ctx->current_feats, ctx->active_feats_mask, &ctx->last_confidence_pct);
    }
    return 0;
#endif
}
