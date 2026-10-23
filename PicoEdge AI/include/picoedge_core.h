#ifndef PICOEDGE_CORE_H
#define PICOEDGE_CORE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#ifdef __ZEPHYR__
#include <zephyr/autoconf.h>
#endif

#ifndef CONFIG_PE_WINDOW_SIZE
#define CONFIG_PE_WINDOW_SIZE 128
#endif
#ifndef CONFIG_PE_AXES
#define CONFIG_PE_AXES 3
#endif
#ifndef CONFIG_PE_MAX_REGIMES
#define CONFIG_PE_MAX_REGIMES 4
#endif
#ifndef CONFIG_PE_MAX_CLASSES
#define CONFIG_PE_MAX_CLASSES 6
#endif
#ifndef CONFIG_PE_MIN_LEARN_WINDOWS
#define CONFIG_PE_MIN_LEARN_WINDOWS 25
#endif
#ifndef CONFIG_PE_ANOMALY_THRESH_PCT
#define CONFIG_PE_ANOMALY_THRESH_PCT 75
#endif
#ifndef CONFIG_PE_HYSTERESIS_CLEAR_PCT
#define CONFIG_PE_HYSTERESIS_CLEAR_PCT 85
#endif
#ifndef CONFIG_PE_DEBOUNCE_WINDOWS
#define CONFIG_PE_DEBOUNCE_WINDOWS 3
#endif

#define PE_FEATS_PER_AXIS 10
#define PE_NUM_FEATURES (CONFIG_PE_AXES * PE_FEATS_PER_AXIS)
#define PE_EPSILON 1e-5f
#define PE_REGIME_SPAWN_SIGMA 2.5f
#define PE_NOMINAL_SIGMA_FLOOR 1.3f
#define PE_MAGIC_HEADER 0x4851U
#define PE_KNOWLEDGE_MAGIC 0x50454B31UL
#define PE_CENTROID_MAGIC 0x50454331UL
#define PE_PKG_MAGIC 0x50455731UL

typedef enum {
    PE_CORE_OK = 0,
    PE_CORE_LEARNING = 1,
    PE_CORE_READY = 2,
    PE_CORE_ERR_UNTRAINED = -1,
    PE_CORE_ERR_INVALID = -2
} pe_core_status_t;

typedef struct {
    uint32_t total_samples;
    uint32_t min_samples;
    float    sensitivity;
    uint8_t  active_regimes;
    uint8_t  reserved[3];
    float    global_mean[PE_NUM_FEATURES];
    float    global_m2[PE_NUM_FEATURES];
    float    centroids[CONFIG_PE_MAX_REGIMES][PE_NUM_FEATURES];
    uint32_t regime_counts[CONFIG_PE_MAX_REGIMES];
} pe_ad_engine_t;

typedef enum {
    PE_CLF_MODE_STATIC_AUTOML = 0,
    PE_CLF_MODE_HOTLOADED_LINEAR = 1
} pe_clf_weight_mode_t;

typedef struct {
    uint32_t total_learned;
    uint8_t  weight_mode;                  // pe_clf_weight_mode_t
    uint8_t  num_hot_classes;
    uint8_t  class_active[CONFIG_PE_MAX_CLASSES];
    uint32_t class_counts[CONFIG_PE_MAX_CLASSES];
    // Shared storage: holds On-Device RBF Prototypes OR Hotloaded Linear SVM/Softmax Weights
    float    prototypes[CONFIG_PE_MAX_CLASSES][PE_NUM_FEATURES];
    float    feat_mean[PE_NUM_FEATURES];   // Used when weight_mode == PE_CLF_MODE_HOTLOADED_LINEAR
    float    feat_var[PE_NUM_FEATURES];    // RBF variance OR Hotloaded scaler_std
    float    bias[CONFIG_PE_MAX_CLASSES];  // Used when weight_mode == PE_CLF_MODE_HOTLOADED_LINEAR
} pe_clf_engine_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;       // PE_KNOWLEDGE_MAGIC (0x50454B31)
    uint16_t version;     // 0x0001
    uint16_t payload_len; // sizeof(pe_ad_engine_t) + sizeof(pe_clf_engine_t)
    pe_ad_engine_t  ad;
    pe_clf_engine_t clf;
    uint16_t crc16;       // CRC-16-CCITT over all preceding bytes
} pe_knowledge_blob_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;       // PE_CENTROID_MAGIC (0x50454331)
    uint8_t  is_fault_prototype; // 0 = Regime Centroid, 1 = Fault Prototype
    uint8_t  slot_id;     // Regime index or Class ID
    uint16_t reserved;
    uint32_t count;
    float    vector[PE_NUM_FEATURES];
    float    variance[PE_NUM_FEATURES];
    uint16_t crc16;
} pe_centroid_blob_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;            // PE_PKG_MAGIC (0x50455731)
    uint8_t  num_classes;      // <= CONFIG_PE_MAX_CLASSES
    uint8_t   reserved[3];
    uint64_t active_feats_mask;
    float    feat_mean[PE_NUM_FEATURES];
    float    feat_std[PE_NUM_FEATURES];
    float    weights[CONFIG_PE_MAX_CLASSES][PE_NUM_FEATURES];
    float    bias[CONFIG_PE_MAX_CLASSES];
    uint16_t crc16;
} pe_pkg_blob_t;

void pe_extract_features_masked(const float *raw_window, float *out_feats, uint64_t active_feats_mask);
void pe_core_ad_init(pe_ad_engine_t *eng, uint32_t min_samples, float sensitivity);
pe_core_status_t pe_core_ad_learn(pe_ad_engine_t *eng, const float *feats, uint64_t mask);
pe_core_status_t pe_core_ad_detect(const pe_ad_engine_t *eng, const float *feats, uint64_t mask, float *similarity_pct, uint8_t *matched_regime, uint8_t *most_deviant_feat);
void pe_core_clf_init(pe_clf_engine_t *clf);
pe_core_status_t pe_core_clf_learn(pe_clf_engine_t *clf, const float *feats, uint8_t class_id);
uint8_t pe_core_clf_predict(const pe_clf_engine_t *clf, const float *feats, uint64_t mask, float *out_confidence_pct);
uint16_t pe_crc16_ccitt(const uint8_t *data, size_t len);

#endif // PICOEDGE_CORE_H
