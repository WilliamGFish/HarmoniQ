#include <sys/types.h>
#ifndef PICOEDGE_H
#define PICOEDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#include <zephyr/smf.h>
#else
// Stub smf_ctx for non-Zephyr builds
struct smf_ctx {
    int state;
};
#endif

#include "picoedge_core.h"

typedef enum {
    PE_EVT_BASELINE_READY = 0,
    PE_EVT_REGIME_SHIFT,
    PE_EVT_FAULT_CONFIRMED,
    PE_EVT_FAULT_CLEARED
} pe_event_type_t;

typedef enum {
    PE_MODE_MONITOR = 0,
    PE_MODE_LEARN_NORMAL,
    PE_MODE_LEARN_FAULT_CLASS
} pe_op_mode_t;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  node_id;
    uint8_t  event_flags;
    uint8_t  similarity_pct;
    uint8_t  active_regime;
    uint8_t  predicted_class;
    uint8_t  class_confidence;
    uint8_t  most_deviant_feat;
    uint8_t  anomaly_streak;
    uint16_t seq_num;
} pe_diag_frame_t;

_Static_assert(sizeof(pe_diag_frame_t) == 12, "pe_diag_frame_t must be 12 bytes");

typedef void (*pe_event_cb_t)(pe_event_type_t evt, const pe_diag_frame_t *frame, void *user_data);

struct pe_context {
    struct smf_ctx   smf;                  // MUST be first member for Zephyr SMF
    pe_ad_engine_t   ad;
    pe_clf_engine_t  clf;
    pe_event_cb_t    event_cb;
    void            *user_data;
    uint64_t         active_feats_mask;
    float            current_feats[PE_NUM_FEATURES];
    float            last_similarity_pct;
    float            last_confidence_pct;
    uint8_t          node_id;
    uint8_t          op_mode;              // pe_op_mode_t
    uint8_t          target_learn_class;
    uint8_t          active_regime;
    uint8_t          prev_regime;
    uint8_t          predicted_class;
    uint8_t          most_deviant_feat;
    uint8_t          anomaly_streak;
    uint8_t          recovery_streak;
    float            goertzel_coeffs[4];
    uint8_t          flags;                // Bit 0: just_primed, Bit 1: from_fault, Bit 2: silent_return
    uint16_t         seq_num;
};

_Static_assert(sizeof(struct pe_context) <= 2048, "pe_context exceeds 2KB RAM budget");

int pe_init(struct pe_context *ctx, uint8_t node_id, pe_event_cb_t cb, void *user_data);
int pe_set_mode(struct pe_context *ctx, pe_op_mode_t mode, uint8_t target_fault_class);
int pe_process_window(struct pe_context *ctx, const float *raw_sensor_window);
ssize_t pe_export_knowledge(const struct pe_context *ctx, uint8_t *out_buf, size_t max_len);
int pe_import_knowledge(struct pe_context *ctx, const uint8_t *in_buf, size_t len);
ssize_t pe_export_centroid(const struct pe_context *ctx, bool is_fault_proto, uint8_t slot_id, uint8_t *out_buf, size_t max_len);
int pe_import_centroid(struct pe_context *ctx, const uint8_t *in_buf, size_t len);
int pe_hotload_weights(struct pe_context *ctx, const uint8_t *pe_pkg_blob, size_t len);
int pe_set_goertzel_freqs(struct pe_context *ctx, const float freqs_hz[4], float sample_rate_hz);

#endif // PICOEDGE_H
