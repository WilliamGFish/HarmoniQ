#include "picoedge_core.h"
#include "picoedge_hal.h"
#include "picoedge_model.h"
#include <math.h>

void pe_core_ad_init(pe_ad_engine_t *eng, uint32_t min_samples, float sensitivity) {
    memset(eng, 0, sizeof(*eng));
    eng->min_samples = (min_samples < 5U) ? 5U : min_samples;
    eng->sensitivity = (sensitivity <= 0.0f) ? 1.0f : sensitivity;
}

static float pe_mahalanobis_dist(const pe_ad_engine_t *eng, const float *f, const float *centroid, uint64_t mask, uint8_t *worst_feat_out) {
    uint32_t denom = (eng->total_samples > 1U) ? (eng->total_samples - 1U) : 1U;
    float dist_sq = 0.0f;
    uint32_t active_count = 0;
    float max_z_sq = -1.0f;
    uint8_t worst_feat = 0;

    for (int i = 0; i < PE_NUM_FEATURES; i++) {
        if (((mask >> i) & 1U) == 0) continue;
        float var = eng->global_m2[i] / (float)denom;
        float diff = f[i] - centroid[i];
        float z_sq = (diff * diff) / (var + PE_EPSILON);
        if (z_sq > max_z_sq) {
            max_z_sq = z_sq;
            worst_feat = i;
        }
        dist_sq += z_sq;
        active_count++;
    }

    if (worst_feat_out) *worst_feat_out = worst_feat;
    return sqrtf(dist_sq / (float)(active_count > 0 ? active_count : 1));
}

pe_core_status_t pe_core_ad_learn(pe_ad_engine_t *eng, const float *feats, uint64_t mask) {
    (void)mask; // Mask is used in detect, but all active features are learned in global stats
    eng->total_samples++;

    // Welford's online algorithm
    for (int i = 0; i < PE_NUM_FEATURES; i++) {
        float delta = feats[i] - eng->global_mean[i];
        eng->global_mean[i] += delta / (float)eng->total_samples;
        float delta2 = feats[i] - eng->global_mean[i];
        eng->global_m2[i] += delta * delta2;
    }

    if (eng->active_regimes == 0) {
        memcpy(eng->centroids[0], feats, PE_NUM_FEATURES * sizeof(float));
        eng->regime_counts[0] = 1;
        eng->active_regimes = 1;
    } else {
        float min_d = INFINITY;
        uint8_t nearest_r = 0;
        for (uint8_t r = 0; r < eng->active_regimes; r++) {
            float d = pe_mahalanobis_dist(eng, feats, eng->centroids[r], mask, NULL);
            if (d < min_d) {
                min_d = d;
                nearest_r = r;
            }
        }

        if (min_d > PE_REGIME_SPAWN_SIGMA && eng->active_regimes < CONFIG_PE_MAX_REGIMES && eng->total_samples > 3U) {
            uint8_t new_r = eng->active_regimes++;
            memcpy(eng->centroids[new_r], feats, PE_NUM_FEATURES * sizeof(float));
            eng->regime_counts[new_r] = 1;
        } else {
            eng->regime_counts[nearest_r]++;
            for (int i = 0; i < PE_NUM_FEATURES; i++) {
                eng->centroids[nearest_r][i] += (feats[i] - eng->centroids[nearest_r][i]) / (float)eng->regime_counts[nearest_r];
            }
        }
    }

    return (eng->total_samples >= eng->min_samples) ? PE_CORE_READY : PE_CORE_LEARNING;
}

pe_core_status_t pe_core_ad_detect(const pe_ad_engine_t *eng, const float *feats, uint64_t mask, float *similarity_pct, uint8_t *matched_regime, uint8_t *most_deviant_feat) {
    if (eng->total_samples < eng->min_samples || eng->active_regimes == 0) {
        return PE_CORE_ERR_UNTRAINED;
    }

    float min_d = INFINITY;
    uint8_t best_r = 0;
    uint8_t best_worst_f = 0;

    for (uint8_t r = 0; r < eng->active_regimes; r++) {
        uint8_t worst_f = 0;
        float d = pe_mahalanobis_dist(eng, feats, eng->centroids[r], mask, &worst_f);
        if (d < min_d) {
            min_d = d;
            best_r = r;
            best_worst_f = worst_f;
        }
    }

    if (matched_regime) *matched_regime = best_r;
    if (most_deviant_feat) *most_deviant_feat = best_worst_f;

    float excess = (min_d > PE_NOMINAL_SIGMA_FLOOR) ? (min_d - PE_NOMINAL_SIGMA_FLOOR) : 0.0f;
    float sim = 100.0f * expf(-0.4f * eng->sensitivity * excess);
    if (sim < 0.0f) sim = 0.0f;
    if (sim > 100.0f) sim = 100.0f;

    if (similarity_pct) *similarity_pct = sim;

    return PE_CORE_OK;
}

void pe_core_clf_init(pe_clf_engine_t *clf) {
    memset(clf, 0, sizeof(*clf));
    clf->weight_mode = PE_CLF_MODE_STATIC_AUTOML;
    for (int i = 0; i < PE_NUM_FEATURES; i++) {
        clf->feat_var[i] = 1.0f;
    }
}

pe_core_status_t pe_core_clf_learn(pe_clf_engine_t *clf, const float *feats, uint8_t class_id) {
    if (class_id >= CONFIG_PE_MAX_CLASSES) return PE_CORE_ERR_INVALID;

    if (clf->weight_mode == PE_CLF_MODE_HOTLOADED_LINEAR) {
        pe_core_clf_init(clf);
    }

    clf->class_active[class_id] = 1;
    clf->class_counts[class_id]++;
    clf->total_learned++;

    for (int i = 0; i < PE_NUM_FEATURES; i++) {
        float diff = feats[i] - clf->prototypes[class_id][i];
        clf->prototypes[class_id][i] += diff / (float)clf->class_counts[class_id];

        if (clf->total_learned > 2) {
            clf->feat_var[i] = 0.9f * clf->feat_var[i] + 0.1f * (diff * diff);
        }
    }
    return PE_CORE_OK;
}

uint8_t pe_core_clf_predict(const pe_clf_engine_t *clf, const float *feats, uint64_t mask, float *out_confidence_pct) {
    (void)mask;
    if (clf->weight_mode == PE_CLF_MODE_HOTLOADED_LINEAR) {
        float f_std[PE_NUM_FEATURES];
        for (int i = 0; i < PE_NUM_FEATURES; i++) {
            f_std[i] = (feats[i] - clf->feat_mean[i]) / (clf->feat_var[i] + PE_EPSILON);
        }

        float logit[CONFIG_PE_MAX_CLASSES];
        float max_logit = -INFINITY;
        for (int c = 0; c < clf->num_hot_classes; c++) {
            logit[c] = clf->bias[c] + pe_hal_dot_prod_f32(f_std, clf->prototypes[c], PE_NUM_FEATURES);
            if (logit[c] > max_logit) max_logit = logit[c];
        }

        float sum_exp = 0.0f;
        float prob[CONFIG_PE_MAX_CLASSES];
        uint8_t best_c = 0;
        float max_prob = -1.0f;

        for (int c = 0; c < clf->num_hot_classes; c++) {
            prob[c] = expf(logit[c] - max_logit);
            sum_exp += prob[c];
        }

        for (int c = 0; c < clf->num_hot_classes; c++) {
            prob[c] /= (sum_exp + PE_EPSILON);
            if (prob[c] > max_prob) {
                max_prob = prob[c];
                best_c = c;
            }
        }

        if (out_confidence_pct) *out_confidence_pct = max_prob * 100.0f;
        return best_c;
    } else {
        float auto_conf = 0.0f;
        uint8_t auto_c = pe_generated_model_predict(feats, &auto_conf);

        if (clf->total_learned == 0) {
            if (out_confidence_pct) *out_confidence_pct = auto_conf * 100.0f;
            return auto_c;
        }

        float max_rbf_score = -1.0f;
        uint8_t best_rbf_class = 0;

        for (uint8_t c = 0; c < CONFIG_PE_MAX_CLASSES; c++) {
            if (clf->class_active[c]) {
                float dist_sq = 0.0f;
                for (int i = 0; i < PE_NUM_FEATURES; i++) {
                    float diff = feats[i] - clf->prototypes[c][i];
                    dist_sq += (diff * diff) / (clf->feat_var[i] + PE_EPSILON);
                }
                float rbf_score = expf(-0.25f * sqrtf(dist_sq));
                if (rbf_score > max_rbf_score) {
                    max_rbf_score = rbf_score;
                    best_rbf_class = c;
                }
            }
        }

        if (max_rbf_score > 0.60f) {
            if (out_confidence_pct) *out_confidence_pct = max_rbf_score * 100.0f;
            return best_rbf_class;
        } else {
            if (out_confidence_pct) *out_confidence_pct = auto_conf * 100.0f;
            return auto_c;
        }
    }
}
