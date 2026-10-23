# DectroniQ HarmoniQ™ — PicoEdge AI™ Library & Studio
## Exhaustive Agentic Work Breakdown Structure (WBS) & Implementation Specification

> **Instructions for the Agentic Harness:** Execute the tasks below sequentially. Do not deviate from the struct definitions, function signatures, magic numbers, state transition rules, or file paths defined in this document. After completing each WBS task, execute its **Verification Gate** command before proceeding to the next task.

---

## PHASE 1: Repository Infrastructure, Build System & Exact Header Contracts

### WBS 1.1: Workspace Tree & Zephyr Module Manifest
* **Task ID:** `WBS 1.1.1`
* **Target Files:** `zephyr/module.yml`, `.gitignore`
* **Dependencies:** None
* **Implementation Details:**
  1. Create directory tree: `zephyr/`, `include/`, `src/`, `studio/`, `studio/tests/`, `tests/ztest_picoedge/src/`, `samples/nrf9161dk_bench/src/`, `scripts/`.
  2. Write `zephyr/module.yml` with exact contents:
     ```yaml
     name: picoedge_ai
     build:
       cmake: .
       kconfig: Kconfig
     ```
  3. Write `.gitignore` excluding `build/`, `__pycache__/`, `*.so`, `*.o`, `*.a`, `*.pe_pkg`, `*.npz`, `.pytest_cache/`.
* **Verification Gate:** `test -f zephyr/module.yml && python3 -c "import yaml; yaml.safe_load(open('zephyr/module.yml'))"`

---

### WBS 1.2: Zephyr Parametric `Kconfig`
* **Task ID:** `WBS 1.2.1`
* **Target Files:** `Kconfig`
* **Dependencies:** `WBS 1.1.1`
* **Implementation Details:**
  Write `Kconfig` with exact symbols and defaults:
  * `config PICOEDGE_AI` (`bool "DectroniQ PicoEdge AI Embedded TinyML Library"`, `default n`, `select SMF`, `select SMF_ANCESTOR_SUPPORT`)
  * Inside `if PICOEDGE_AI`:
    * `config PE_WINDOW_SIZE` (`int`, `default 128`, `range 16 1024`)
    * `config PE_AXES` (`int`, `default 3`, `range 1 6`)
    * `config PE_MAX_REGIMES` (`int`, `default 4`, `range 1 8`)
    * `config PE_MAX_CLASSES` (`int`, `default 6`, `range 2 16`)
    * `config PE_MIN_LEARN_WINDOWS` (`int`, `default 25`, `range 5 1000`)
    * `config PE_ANOMALY_THRESH_PCT` (`int`, `default 75`, `range 1 99`)
    * `config PE_HYSTERESIS_CLEAR_PCT` (`int`, `default 85`, `range 2 100`)
    * `config PE_DEBOUNCE_WINDOWS` (`int`, `default 3`, `range 1 32`)
* **Verification Gate:** Verify file contains all 9 `config` symbols via `grep -c "^config " Kconfig` returning `9`.

---

### WBS 1.3: Zephyr `CMakeLists.txt`
* **Task ID:** `WBS 1.3.1`
* **Target Files:** `CMakeLists.txt`
* **Dependencies:** `WBS 1.2.1`
* **Implementation Details:**
  Write `CMakeLists.txt`:
  ```cmake
  if(CONFIG_PICOEDGE_AI)
    zephyr_include_directories(include)
    zephyr_library_named(picoedge_ai)
    zephyr_library_sources(
      src/picoedge_dsp.c
      src/picoedge_core.c
      src/picoedge_serde.c
      src/picoedge_smf.c
    )
    zephyr_library_compile_options(-Os -Wall -Wextra -Werror)
  endif()
  ```
* **Verification Gate:** File exists and syntax is valid CMake.

---

### WBS 1.4: Exact Header Contracts (`include/*.h`)

#### WBS 1.4.1: Layer 1 OS-Agnostic Core Header (`include/picoedge_core.h`)
* **Target Files:** `include/picoedge_core.h`
* **Dependencies:** `WBS 1.1.1`
* **Implementation Details:**
  1. Must NOT include any `<zephyr/*>` headers so it can be compiled standalone by host `gcc` for Python `ctypes` parity testing.
  2. Provide fallback macros when compiled outside Zephyr (where `CONFIG_PE_*` are not injected by `autoconf.h`):
     ```c
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
     ```
  3. Define exact constants:
     * `#define PE_FEATS_PER_AXIS 10`
     * `#define PE_NUM_FEATURES (CONFIG_PE_AXES * PE_FEATS_PER_AXIS)` (30 by default)
     * `#define PE_EPSILON 1e-5f`
     * `#define PE_REGIME_SPAWN_SIGMA 2.5f`
     * `#define PE_NOMINAL_SIGMA_FLOOR 1.3f`
     * `#define PE_MAGIC_HEADER 0x4851U` (`"HQ"`)
     * `#define PE_KNOWLEDGE_MAGIC 0x50454B31UL` (`"PEK1"`)
     * `#define PE_CENTROID_MAGIC 0x50454331UL` (`"PEC1"`)
     * `#define PE_PKG_MAGIC 0x50455731UL` (`"PEW1"`)
  4. Define exact Layer 1 structs (sized to keep total context `< 2,048 Bytes`):
     ```c
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
     } pe_ad_engine_t; // 752 Bytes @ 30 feats, 4 regimes

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
     } pe_clf_engine_t; // 1,020 Bytes @ 30 feats, 6 classes
     ```
  5. Define exact serialization wire structs (`__attribute__((packed))`):
     ```c
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
         uint32_t active_feats_mask;
         float    feat_mean[PE_NUM_FEATURES];
         float    feat_std[PE_NUM_FEATURES];
         float    weights[CONFIG_PE_MAX_CLASSES][PE_NUM_FEATURES];
         float    bias[CONFIG_PE_MAX_CLASSES];
         uint16_t crc16;
     } pe_pkg_blob_t;
     ```
  6. Declare all Layer 1 functions (`pe_extract_features_masked`, `pe_core_ad_init`, `pe_core_ad_learn`, `pe_core_ad_detect`, `pe_core_clf_init`, `pe_core_clf_learn`, `pe_core_clf_predict`, `pe_crc16_ccitt`).
* **Verification Gate:** `gcc -std=c99 -Wall -Wextra -Werror -Iinclude -fsyntax-only include/picoedge_core.h`

#### WBS 1.4.2: Public Host Firmware API Header (`include/picoedge.h`)
* **Target Files:** `include/picoedge.h`
* **Dependencies:** `WBS 1.4.1`
* **Implementation Details:**
  1. Include `<zephyr/kernel.h>`, `<zephyr/smf.h>`, and `"picoedge_core.h"`.
  2. Define `pe_event_type_t`, `pe_op_mode_t`, `pe_diag_frame_t` (12 bytes packed), and `pe_event_cb_t` exactly as specified in the architecture blueprint.
  3. Add compile-time size assertions:
     ```c
     _Static_assert(sizeof(pe_diag_frame_t) == 12, "pe_diag_frame_t must be 12 bytes");
     ```
  4. Define `struct pe_context`:
     ```c
     struct pe_context {
         struct smf_ctx   smf;                  // MUST be first member for Zephyr SMF
         pe_ad_engine_t   ad;
         pe_clf_engine_t  clf;
         pe_event_cb_t    event_cb;
         void            *user_data;
         uint32_t         active_feats_mask;
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
         uint8_t          flags;                // Bit 0: just_primed, Bit 1: from_fault, Bit 2: silent_return
         uint16_t         seq_num;
     };
     _Static_assert(sizeof(struct pe_context) <= 2048, "pe_context exceeds 2KB RAM budget");
     ```
  5. Declare all public functions:
     * `int pe_init(struct pe_context *ctx, uint8_t node_id, pe_event_cb_t cb, void *user_data);`
     * `int pe_set_mode(struct pe_context *ctx, pe_op_mode_t mode, uint8_t target_fault_class);`
     * `int pe_process_window(struct pe_context *ctx, const float *raw_sensor_window);`
     * `ssize_t pe_export_knowledge(const struct pe_context *ctx, uint8_t *out_buf, size_t max_len);`
     * `int pe_import_knowledge(struct pe_context *ctx, const uint8_t *in_buf, size_t len);`
     * `ssize_t pe_export_centroid(const struct pe_context *ctx, bool is_fault_proto, uint8_t slot_id, uint8_t *out_buf, size_t max_len);`
     * `int pe_import_centroid(struct pe_context *ctx, const uint8_t *in_buf, size_t len);`
     * `int pe_hotload_weights(struct pe_context *ctx, const uint8_t *pe_pkg_blob, size_t len);`

#### WBS 1.4.3: Cross-ISA Hardware DSP Header (`include/picoedge_hal.h`)
* **Target Files:** `include/picoedge_hal.h`
* **Dependencies:** `WBS 1.4.1`
* **Implementation Details:**
  Write `static inline float pe_hal_dot_prod_f32(const float *a, const float *b, uint32_t len)`:
  * If `defined(CONFIG_CMSIS_DSP) && defined(__ARM_ARCH_8M_MAIN__)`: include `<arm_math.h>` and call `arm_dot_prod_f32(a, b, len, &res)`.
  * Else if `defined(__riscv)`: 4-way unrolled accumulation loop (`sum0 + sum1`) to pipeline `fmadd.s`.
  * Else: standard C99 `for` loop.

#### WBS 1.4.4: Default Bootstrap Model Header (`include/picoedge_model.h`)
* **Target Files:** `include/picoedge_model.h`
* **Dependencies:** `WBS 1.4.1`
* **Implementation Details:**
  Create an initial valid `include/picoedge_model.h` defining:
  * `#define PE_DEFAULT_ACTIVE_FEATS_MASK 0x3FFFFFFFUL` (all 30 bits active)
  * `static inline uint8_t pe_generated_model_predict(const float *feats, float *out_confidence)` which implements a default fallback rule (and will be overwritten by `studio/harmoniq_studio.py` in Phase 4).
* **Verification Gate:** `gcc -std=c99 -Wall -Wextra -Werror -Iinclude -fsyntax-only include/picoedge_model.h`

---

## PHASE 2: Layer 1 — Pure ANSI C99 Microsecond Inner Loop

### WBS 2.1: Single-Pass Hybrid Feature Extractor (`src/picoedge_dsp.c`)
* **Task ID:** `WBS 2.1.1`
* **Target Files:** `src/picoedge_dsp.c`
* **Dependencies:** `WBS 1.4.1`, `WBS 1.4.3`
* **Implementation Details:**
  Implement `void pe_extract_features_masked(const float *raw_window, float *out_feats, uint32_t active_feats_mask)`:
  1. Define `static const float GOERTZEL_COEFFS[4] = { 1.902113f, 1.457937f, 0.000000f, -1.618034f };`
  2. Loop `for (int ch = 0; ch < CONFIG_PE_AXES; ch++)`:
     * Let `int base = ch * PE_FEATS_PER_AXIS;`
     * Check if any bit for this axis (`(active_feats_mask >> base) & 0x3FFU`) is non-zero; if `0`, zero out `out_feats[base .. base+9]` and `continue`.
     * **Pass 1:** Loop `i = 0 .. CONFIG_PE_WINDOW_SIZE - 1`, indexing `val = raw_window[i * CONFIG_PE_AXES + ch]`. Accumulate `sum`, `sq_sum`, `min_v`, `max_v`.
     * Compute `mean = sum / (float)CONFIG_PE_WINDOW_SIZE;` and `rms = sqrtf(sq_sum / (float)CONFIG_PE_WINDOW_SIZE);`.
     * **Pass 2:** Initialize `var_sum = 0.0f`, `fourth_moment = 0.0f`, `zcr_count = 0`, `prev_diff = raw_window[ch] - mean`, `s1[4] = {0}`, `s2[4] = {0}`.
     * Check `uint32_t spec_mask = (active_feats_mask >> (base + 6)) & 0x0FU;`.
     * Loop `i = 0 .. CONFIG_PE_WINDOW_SIZE - 1`:
       * `float diff = raw_window[i * CONFIG_PE_AXES + ch] - mean;`
       * `float d2 = diff * diff;`
       * `var_sum += d2;`
       * `fourth_moment += d2 * d2;`
       * If `i > 0 && ((diff >= 0.0f && prev_diff < 0.0f) || (diff < 0.0f && prev_diff >= 0.0f))` increment `zcr_count`.
       * `prev_diff = diff;`
       * If `spec_mask != 0`: for `b = 0..3`, compute `s0 = diff + GOERTZEL_COEFFS[b] * s1[b] - s2[b]; s2[b] = s1[b]; s1[b] = s0;`.
     * Compute:
       * `variance = var_sum / (float)CONFIG_PE_WINDOW_SIZE;`
       * `std_dev = sqrtf(variance > 0.0f ? variance : 0.0f);`
       * `kurtosis = (fourth_moment / (float)CONFIG_PE_WINDOW_SIZE) / (variance * variance + PE_EPSILON);`
       * `max_abs = (fabsf(max_v) > fabsf(min_v)) ? fabsf(max_v) : fabsf(min_v);`
       * `crest = max_abs / (rms + PE_EPSILON);`
       * `zcr = (float)zcr_count / (float)CONFIG_PE_WINDOW_SIZE;`
     * Store `out_feats[base + 0..5]`, and for `b = 0..3`, compute `power = s1[b]*s1[b] + s2[b]*s2[b] - GOERTZEL_COEFFS[b]*s1[b]*s2[b];` and store `out_feats[base + 6 + b] = log10f((fabsf(power) / (float)CONFIG_PE_WINDOW_SIZE) + 1.0f);`.
     * Zero any individual feature index `k` where `((active_feats_mask >> k) & 1U) == 0`.
* **Verification Gate:** `gcc -std=c99 -O2 -Wall -Wextra -Werror -Iinclude -c src/picoedge_dsp.c -o /tmp/picoedge_dsp.o`

---

### WBS 2.2: Multi-Regime Unsupervised Anomaly Engine (`src/picoedge_core.c` Part 1)
* **Task ID:** `WBS 2.2.1`
* **Target Files:** `src/picoedge_core.c`
* **Dependencies:** `WBS 2.1.1`
* **Implementation Details:**
  1. `void pe_core_ad_init(pe_ad_engine_t *eng, uint32_t min_samples, float sensitivity)`:
     * `memset(eng, 0, sizeof(*eng));`
     * `eng->min_samples = (min_samples < 5U) ? 5U : min_samples;`
     * `eng->sensitivity = (sensitivity <= 0.0f) ? 1.0f : sensitivity;`
  2. Helper `static float pe_mahalanobis_dist(const pe_ad_engine_t *eng, const float *f, const float *centroid, uint32_t mask, uint8_t *worst_feat_out)`:
     * `uint32_t denom = (eng->total_samples > 1U) ? (eng->total_samples - 1U) : 1U;`
     * Loop `i = 0 .. PE_NUM_FEATURES - 1`: skip if `((mask >> i) & 1U) == 0`.
     * `float var = eng->global_m2[i] / (float)denom;`
     * `float diff = f[i] - centroid[i];`
     * `float z_sq = (diff * diff) / (var + PE_EPSILON);`
     * Track maximum `z_sq` index in `*worst_feat_out`, accumulate `dist_sq += z_sq`, increment `active_count`.
     * Return `sqrtf(dist_sq / (float)(active_count > 0 ? active_count : 1))`.
  3. `pe_core_status_t pe_core_ad_learn(pe_ad_engine_t *eng, const float *feats, uint32_t mask)`:
     * Increment `eng->total_samples`.
     * Update Welford `global_mean[i]` and `global_m2[i]` for all `i`.
     * If `eng->active_regimes == 0`: copy `feats` to `eng->centroids[0]`, set `regime_counts[0] = 1`, `active_regimes = 1`.
     * Else: find nearest existing regime `nearest_r` with distance `min_d`.
       * If `min_d > PE_REGIME_SPAWN_SIGMA && eng->active_regimes < CONFIG_PE_MAX_REGIMES && eng->total_samples > 3U`:
         * Spawn new regime `new_r = eng->active_regimes++`, copy `feats` to `centroids[new_r]`, set `regime_counts[new_r] = 1`.
       * Else:
         * Increment `regime_counts[nearest_r]`; update `centroids[nearest_r][i] += (feats[i] - centroids[nearest_r][i]) / (float)regime_counts[nearest_r]`.
     * Return `(eng->total_samples >= eng->min_samples) ? PE_CORE_READY : PE_CORE_LEARNING`.
  4. `pe_core_status_t pe_core_ad_detect(const pe_ad_engine_t *eng, const float *feats, uint32_t mask, float *similarity_pct, uint8_t *matched_regime, uint8_t *most_deviant_feat)`:
     * If `eng->total_samples < eng->min_samples || eng->active_regimes == 0`: return `PE_CORE_ERR_UNTRAINED`.
     * Find regime `best_r` minimizing `pe_mahalanobis_dist(eng, feats, eng->centroids[r], mask, &worst_f)`.
     * Set `*matched_regime = best_r` and `*most_deviant_feat = best_worst_f`.
     * Compute `float excess = (min_d > PE_NOMINAL_SIGMA_FLOOR) ? (min_d - PE_NOMINAL_SIGMA_FLOOR) : 0.0f;`
     * Compute `float sim = 100.0f * expf(-0.4f * eng->sensitivity * excess);`
     * Clamp `sim` to `[0.0f, 100.0f]` and write `*similarity_pct = sim`. Return `PE_CORE_OK`.

---

### WBS 2.3: Dual-Mode Fault Classifier (`src/picoedge_core.c` Part 2)
* **Task ID:** `WBS 2.3.1`
* **Target Files:** `src/picoedge_core.c`
* **Dependencies:** `WBS 2.2.1`, `WBS 1.4.4`
* **Implementation Details:**
  1. `void pe_core_clf_init(pe_clf_engine_t *clf)`:
     * `memset(clf, 0, sizeof(*clf));`
     * Set `clf->weight_mode = PE_CLF_MODE_STATIC_AUTOML;`
     * Initialize `clf->feat_var[i] = 1.0f` for all `i`.
  2. `pe_core_status_t pe_core_clf_learn(pe_clf_engine_t *clf, const float *feats, uint8_t class_id)`:
     * If `class_id >= CONFIG_PE_MAX_CLASSES` return `PE_CORE_ERR_INVALID`.
     * If `clf->weight_mode == PE_CLF_MODE_HOTLOADED_LINEAR`, reset `clf` to RBF mode first.
     * Set `clf->class_active[class_id] = 1`, increment `class_counts[class_id]` and `total_learned`.
     * Update `prototypes[class_id][i]` via running mean; if `total_learned > 2`, update `feat_var[i] = 0.9f * feat_var[i] + 0.1f * (diff * diff)`.
     * Return `PE_CORE_OK`.
  3. `uint8_t pe_core_clf_predict(const pe_clf_engine_t *clf, const float *feats, uint32_t mask, float *out_confidence_pct)`:
     * **Case 1 (`clf->weight_mode == PE_CLF_MODE_HOTLOADED_LINEAR`):**
       * Standardize `f_std[i] = (feats[i] - clf->feat_mean[i]) / (clf->feat_var[i] + PE_EPSILON)`.
       * For `c = 0 .. clf->num_hot_classes - 1`: compute `logit[c] = clf->bias[c] + pe_hal_dot_prod_f32(f_std, clf->prototypes[c], PE_NUM_FEATURES)`.
       * Apply numerically stable Softmax (`expf(logit[c] - max_logit)`) to obtain winning class `best_c` and `*out_confidence_pct = prob[best_c] * 100.0f`.
       * Return `best_c`.
     * **Case 2 (Default Static AutoML + Optional On-Device RBF Few-Shot Override):**
       * Evaluate compile-time AutoML model: `float auto_conf = 0.0f; uint8_t auto_c = pe_generated_model_predict(feats, &auto_conf);`.
       * If `clf->total_learned == 0`: set `*out_confidence_pct = auto_conf * 100.0f` and return `auto_c`.
       * Else: evaluate RBF kernel `score[c] = expf(-0.25f * mahalanobis_d)` across all `c` where `clf->class_active[c] == 1`. If `max_rbf_score > 0.60f`, return `best_rbf_class` with normalized RBF confidence; else return `auto_c`.
* **Verification Gate:** `gcc -std=c99 -O2 -Wall -Wextra -Werror -Iinclude -c src/picoedge_core.c -o /tmp/picoedge_core.o`

---

### WBS 2.4: CRC-16-CCITT & Binary Serialization (`src/picoedge_serde.c`)
* **Task ID:** `WBS 2.4.1`
* **Target Files:** `src/picoedge_serde.c`
* **Dependencies:** `WBS 2.3.1`, `WBS 1.4.2`
* **Implementation Details:**
  1. `uint16_t pe_crc16_ccitt(const uint8_t *data, size_t len)`:
     * Polynomial `0x1021`, initial value `0xFFFF`.
  2. `ssize_t pe_export_knowledge(const struct pe_context *ctx, uint8_t *out_buf, size_t max_len)`:
     * Validate `ctx != NULL && out_buf != NULL && max_len >= sizeof(pe_knowledge_blob_t)`. If not, return `-EINVAL` (`-22`).
     * Populate `pe_knowledge_blob_t`, compute `crc16` over `offsetof(pe_knowledge_blob_t, crc16)`, copy to `out_buf`, and return `sizeof(pe_knowledge_blob_t)`.
  3. `int pe_import_knowledge(struct pe_context *ctx, const uint8_t *in_buf, size_t len)`:
     * Validate `len == sizeof(pe_knowledge_blob_t)`, `magic == PE_KNOWLEDGE_MAGIC`, `version == 1`, and `crc16` match. Return `-EINVAL` on any mismatch without altering `ctx`.
     * Copy `blob.ad` to `ctx->ad` and `blob.clf` to `ctx->clf`. If `ctx->ad.total_samples >= ctx->ad.min_samples`, transition SMF to `PE_STATE_NOMINAL`. Return `0`.
  4. `ssize_t pe_export_centroid(...)` and `int pe_import_centroid(...)`:
     * Serialize/deserialize a single regime centroid (`is_fault_proto == 0`) or fault prototype (`is_fault_proto == 1`) using `pe_centroid_blob_t` (`PE_CENTROID_MAGIC`, CRC-16).
  5. `int pe_hotload_weights(struct pe_context *ctx, const uint8_t *pe_pkg_blob, size_t len)`:
     * Validate `len == sizeof(pe_pkg_blob_t)`, `magic == PE_PKG_MAGIC`, `num_classes <= CONFIG_PE_MAX_CLASSES`, and `crc16`.
     * Copy `feat_mean`, `feat_std` (into `clf.feat_var`), `weights` (into `clf.prototypes`), `bias`, set `clf.weight_mode = PE_CLF_MODE_HOTLOADED_LINEAR`, `clf.num_hot_classes = blob->num_classes`, and `ctx->active_feats_mask = blob->active_feats_mask`. Return `0`.
* **Verification Gate:** `gcc -std=c99 -O2 -Wall -Wextra -Werror -Iinclude -c src/picoedge_serde.c -o /tmp/picoedge_serde.o` *(using stub/ifdef for `smf_set_state` when compiled outside Zephyr)*.

---

## PHASE 3: Layer 2 — Zephyr SMF Millisecond Outer Loop

### WBS 3.1: Hierarchical Zephyr State Machine & Public API (`src/picoedge_smf.c`)
* **Task ID:** `WBS 3.1.1`
* **Target Files:** `src/picoedge_smf.c`
* **Dependencies:** `WBS 2.4.1`, `WBS 1.4.2`
* **Implementation Details:**
  1. Define bit flags in `ctx->flags`:
     * `#define PE_FLAG_JUST_PRIMED   (1U << 0)`
     * `#define PE_FLAG_FROM_FAULT    (1U << 1)`
     * `#define PE_FLAG_SILENT_RETURN (1U << 2)`
  2. Define `enum pe_smf_state_id`:
     * `PE_STATE_LEARNING = 0`
     * `PE_STATE_MONITOR_PARENT = 1`
     * `PE_STATE_NOMINAL = 2`
     * `PE_STATE_SUSPECT = 3`
     * `PE_STATE_FAULT = 4`
  3. Implement helper `static void pe_emit_event(struct pe_context *ctx, pe_event_type_t evt)`:
     * Increment `ctx->seq_num`.
     * Populate `pe_diag_frame_t frame`:
       * `frame.magic = PE_MAGIC_HEADER;` (`0x4851`)
       * `frame.node_id = ctx->node_id;`
       * `frame.event_flags = ((evt == PE_EVT_FAULT_CONFIRMED) ? 0x01U : 0x00U) | ((evt == PE_EVT_REGIME_SHIFT) ? 0x02U : 0x00U) | ((ctx->ad.total_samples >= ctx->ad.min_samples) ? 0x04U : 0x00U);`
       * `frame.similarity_pct = (uint8_t)(ctx->last_similarity_pct + 0.5f);`
       * `frame.active_regime = ctx->active_regime;`
       * `frame.predicted_class = ctx->predicted_class;`
       * `frame.class_confidence = (uint8_t)(ctx->last_confidence_pct + 0.5f);`
       * `frame.most_deviant_feat = ctx->most_deviant_feat;`
       * `frame.anomaly_streak = ctx->anomaly_streak;`
       * `frame.seq_num = ctx->seq_num;`
     * If `ctx->event_cb != NULL`, invoke `ctx->event_cb(evt, &frame, ctx->user_data);`.
  4. Implement SMF State Callbacks:
     * **`s_learning_run(void *obj)`:**
       * Call `pe_extract_features_masked(raw_window_ptr, ctx->current_feats, ctx->active_feats_mask)`.
       * Call `pe_core_ad_learn(&ctx->ad, ctx->current_feats, ctx->active_feats_mask)`.
       * If result == `PE_CORE_READY`: run `pe_core_ad_detect()` once to populate initial `active_regime` and `last_similarity_pct`, set `ctx->prev_regime = ctx->active_regime`, set `ctx->op_mode = PE_MODE_MONITOR`, set `ctx->flags |= PE_FLAG_JUST_PRIMED`, and call `smf_set_state(SMF_CTX(ctx), &pe_smf_states[PE_STATE_NOMINAL])`.
     * **`s_monitor_parent_run(void *obj)`:**
       * Executes automatically before `NOMINAL`, `SUSPECT`, or `FAULT` `run` handlers!
       * Calls `pe_extract_features_masked(...)`, `pe_core_ad_detect(...)`, and `pe_core_clf_predict(...)`, storing results in `ctx`.
     * **`s_nominal_entry(void *obj)`:**
       * Reset `ctx->anomaly_streak = 0`, `ctx->recovery_streak = 0`.
       * If `ctx->flags & PE_FLAG_SILENT_RETURN`: clear `PE_FLAG_SILENT_RETURN` and **return without firing callback** (prevents debounced glitches from waking the radio!).
       * Else if `ctx->flags & PE_FLAG_JUST_PRIMED`: clear flag, call `pe_emit_event(ctx, PE_EVT_BASELINE_READY)`.
       * Else if `ctx->flags & PE_FLAG_FROM_FAULT`: clear flag, call `pe_emit_event(ctx, PE_EVT_FAULT_CLEARED)`.
     * **`s_nominal_run(void *obj)`:**
       * If `ctx->last_similarity_pct < (float)CONFIG_PE_ANOMALY_THRESH_PCT`:
         * Set `ctx->anomaly_streak = 1`.
         * If `CONFIG_PE_DEBOUNCE_WINDOWS <= 1`: transition directly to `PE_STATE_FAULT`.
         * Else: transition to `PE_STATE_SUSPECT`.
       * Else if `ctx->active_regime != ctx->prev_regime`:
         * Set `ctx->prev_regime = ctx->active_regime`.
         * Call `pe_emit_event(ctx, PE_EVT_REGIME_SHIFT)`.
     * **`s_suspect_run(void *obj)`:**
       * If `ctx->last_similarity_pct < (float)CONFIG_PE_ANOMALY_THRESH_PCT`:
         * Increment `ctx->anomaly_streak`.
         * If `ctx->anomaly_streak >= CONFIG_PE_DEBOUNCE_WINDOWS`: transition to `PE_STATE_FAULT`.
       * Else:
         * Transient glitch subsided! Set `ctx->flags |= PE_FLAG_SILENT_RETURN` and transition to `PE_STATE_NOMINAL`.
     * **`s_fault_entry(void *obj)`:**
       * Set `ctx->recovery_streak = 0`.
       * Set `ctx->flags |= PE_FLAG_FROM_FAULT`.
       * Call `pe_emit_event(ctx, PE_EVT_FAULT_CONFIRMED)`.
     * **`s_fault_run(void *obj)`:**
       * If `ctx->last_similarity_pct >= (float)CONFIG_PE_HYSTERESIS_CLEAR_PCT`:
         * Increment `ctx->recovery_streak`.
         * If `ctx->recovery_streak >= CONFIG_PE_DEBOUNCE_WINDOWS`: transition to `PE_STATE_NOMINAL`.
       * Else:
         * Increment `ctx->anomaly_streak` (saturating at 255) and reset `ctx->recovery_streak = 0`.
         * **Do NOT fire callback** (radio remains silent while latched in fault).
  5. Implement `pe_init()`, `pe_set_mode()`, and `pe_process_window()` in `src/picoedge_smf.c`.

---

## PHASE 4: HarmoniQ Studio™ AutoML & Dual-Layer Code Transpiler

### WBS 4.1: Bit-Accurate Python DSP Mirror (`studio/harmoniq_dsp.py`)
* **Task ID:** `WBS 4.1.1`
* **Target Files:** `studio/harmoniq_dsp.py`, `studio/requirements.txt`
* **Dependencies:** `WBS 2.1.1`
* **Implementation Details:**
  1. Write `studio/requirements.txt` containing `numpy>=1.24.0`, `scikit-learn>=1.3.0`, `pytest>=7.4.0`.
  2. In `studio/harmoniq_dsp.py`, implement `extract_features_py(raw_window: np.ndarray, active_feats_mask: int = 0x3FFFFFFF) -> np.ndarray`:
     * Force `raw_window = np.asarray(raw_window, dtype=np.float32).reshape(window_size, axes)`.
     * Perform the exact float32 operations of `pe_extract_features_masked()` (using `np.float32` scalars for `mean`, `rms`, `var_sum`, `fourth_moment`, and the Goertzel loop `s0 = np.float32(diff + coeff * s1 - s2)` so IEEE-754 rounding matches C99 within `< 1e-5`).

---

### WBS 4.2: Automated Python-to-C99 `ctypes` Parity Test (`studio/tests/test_studio_c_parity.py`)
* **Task ID:** `WBS 4.2.1`
* **Target Files:** `studio/tests/test_studio_c_parity.py`
* **Dependencies:** `WBS 2.3.1`, `WBS 4.1.1`
* **Implementation Details:**
  1. Use `subprocess.check_call` inside a `pytest` fixture to compile `src/picoedge_dsp.c` and `src/picoedge_core.c` into `/tmp/libpicoedge_parity.so`:
     `gcc -shared -fPIC -O2 -std=c99 -Iinclude src/picoedge_dsp.c src/picoedge_core.c -lm -o /tmp/libpicoedge_parity.so`
  2. Bind `pe_extract_features_masked` via `ctypes`.
  3. Test 1 (`test_dsp_bit_parity_random_and_sine`): Generate 50 windows (white noise, multi-tone sines, impulses, DC offsets). Assert `np.allclose(py_feats, c_feats, atol=1e-5, rtol=1e-5)`.
  4. Test 2 (`test_feature_masking_parity`): Test with random 30-bit masks (`0x15555555`, `0x000003FF`, `0x00000000`) and assert exact zeroing parity.
* **Verification Gate:** `pytest studio/tests/test_studio_c_parity.py` exits with code `0`.

---

### WBS 4.3: Synthetic Industrial Dataset Generator (`studio/generate_synthetic_data.py`)
* **Task ID:** `WBS 4.3.1`
* **Target Files:** `studio/generate_synthetic_data.py`
* **Dependencies:** `WBS 4.1.1`
* **Implementation Details:**
  Implement `generate_dataset(samples_per_class=120, seed=42)` returning `(X_raw, X_feats, y_labels)` across 4 classes (`0: Idle`, `1: Healthy_Run`, `2: Shaft_Unbalance`, `3: Bearing_Spall_Impacts`):
  * `Class 0 (Idle)`: Gaussian noise $\mathcal{N}(0, 0.04^2)$.
  * `Class 1 (Healthy_Run)`: Half windows at $f = 0.05 f_s$ (Regime 1), half at $f = 0.12 f_s$ (Regime 2), amplitude $1.0 + \mathcal{N}(0, 0.08^2)$.
  * `Class 2 (Shaft_Unbalance)`: Fundamental $2.5 \sin(2\pi \cdot 0.05 n) + 1.2 \sin(2\pi \cdot 0.12 n) + \mathcal{N}(0, 0.15^2)$.
  * `Class 3 (Bearing_Spall_Impacts)`: Healthy carrier plus sparse high-amplitude high-frequency bursts at $0.40 f_s$ every 16 samples (producing high Kurtosis, Crest Factor, and Band 3 Goertzel power).
* **Verification Gate:** `python3 studio/generate_synthetic_data.py` runs and verifies shapes `(480, 128, 3)` and `(480, 30)`.

---

### WBS 4.4: HarmoniQ Studio AutoML & Dual Transpiler (`studio/harmoniq_studio.py`)
* **Task ID:** `WBS 4.4.1`
* **Target Files:** `studio/harmoniq_studio.py`, `include/picoedge_model.h`
* **Dependencies:** `WBS 4.3.1`
* **Implementation Details:**
  1. Fit `StandardScaler` on `X_feats`.
  2. Run `RFE(estimator=LinearSVC(random_state=42), n_features_to_select=12)` to compute the 30-bit integer `active_feats_mask` (if 12-feature CV accuracy is within 1.5% of 30-feature accuracy, use the pruned 12-feature mask; else use all 30 features).
  3. Benchmark 4 candidate models using 5-fold `StratifiedKFold`:
     * `DecisionTreeClassifier(max_depth=4, random_state=42)`
     * `LinearSVC(max_iter=3000, random_state=42)`
     * `NearestCentroid()`
     * `MLPClassifier(hidden_layer_sizes=(8,), max_iter=1000, random_state=42)`
  4. Print the ASCII leaderboard table (`Model | 5-Fold CV Accuracy | Est. Flash (B) | Est. RAM (B)`).
  5. Transpile the winning model into `include/picoedge_model.h` implementing `static inline uint8_t pe_generated_model_predict(const float *feats, float *out_confidence)`.
  6. Export a binary `.pe_pkg` file (`struct.pack("<IB3xI30f30f180f6fH", ...)` with CRC-16-CCITT matching `pe_crc16_ccitt()`) to `/tmp/test_weights.pe_pkg` (and optionally a C byte array `PE_SAMPLE_PKG_BLOB` inside `tests/ztest_picoedge/src/test_pkg_blob.h` for `ztest`).
* **Verification Gate:** `python3 studio/harmoniq_studio.py --out-header include/picoedge_model.h --out-pkg-header tests/ztest_picoedge/src/test_pkg_blob.h` succeeds with CV accuracy $> 98\%$.

---

## PHASE 5: Automated Zephyr `ztest` Suite, Bench App & Footprint Audit

### WBS 5.1: Zephyr `ztest` Verification Suite (`tests/ztest_picoedge/`)
* **Task ID:** `WBS 5.1.1`
* **Target Files:** `tests/ztest_picoedge/CMakeLists.txt`, `tests/ztest_picoedge/prj.conf`, `tests/ztest_picoedge/src/main.c`
* **Dependencies:** `WBS 3.1.1`, `WBS 4.4.1`
* **Implementation Details:**
  1. `tests/ztest_picoedge/prj.conf`:
     ```ini
     CONFIG_ZTEST=y
     CONFIG_PICOEDGE_AI=y
     CONFIG_PE_MIN_LEARN_WINDOWS=10
     CONFIG_PE_DEBOUNCE_WINDOWS=3
     CONFIG_PE_ANOMALY_THRESH_PCT=75
     CONFIG_PE_HYSTERESIS_CLEAR_PCT=85
     ```
  2. `tests/ztest_picoedge/CMakeLists.txt`:
     Add `list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../..)` before `find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})`.
  3. `tests/ztest_picoedge/src/main.c`: Implement 6 deterministic `ZTEST` cases:
     * `ZTEST(picoedge_suite, test_01_multi_regime_learning_and_shift)`:
       * Feed 5 windows of Idle noise and 5 windows of 12% $f_s$ sine wave (`total = 10 == CONFIG_PE_MIN_LEARN_WINDOWS`).
       * Assert callback fired exactly **once** with `PE_EVT_BASELINE_READY` and `ctx.ad.active_regimes == 2`.
       * Feed 1 window of Idle noise $\rightarrow$ assert `PE_EVT_REGIME_SHIFT` fires once (`active_regime == 0`, `similarity_pct >= 90`).
       * Feed 5 more windows of Idle noise $\rightarrow$ assert **0 callbacks** fired (radio silence in steady state).
     * `ZTEST(picoedge_suite, test_02_transient_glitch_debounce_suppression)`:
       * Inject 2 consecutive bearing-spall impulse windows (`similarity_pct < 75`), then 3 healthy windows.
       * Assert **0 callbacks** fired (because `2 < CONFIG_PE_DEBOUNCE_WINDOWS` and `PE_FLAG_SILENT_RETURN` suppressed `s_nominal_entry`).
     * `ZTEST(picoedge_suite, test_03_confirmed_fault_and_hysteresis_recovery)`:
       * Inject 5 consecutive bearing-spall impulse windows.
       * Assert `PE_EVT_FAULT_CONFIRMED` fired **exactly once** on window #3 (not on #1, #2, #4, or #5), `last_frame.magic == 0x4851`, `last_frame.predicted_class == 3`, and `last_frame.anomaly_streak == 3`.
       * Inject 3 healthy windows (`similarity_pct >= 85`). Assert `PE_EVT_FAULT_CLEARED` fires on healthy window #3.
     * `ZTEST(picoedge_suite, test_04_in_situ_few_shot_fault_learning)`:
       * Call `pe_set_mode(&ctx, PE_MODE_LEARN_FAULT_CLASS, 5)`. Feed 5 windows of a 40% $f_s$ high-frequency fault.
       * Return to `PE_MODE_MONITOR`, inject 3 windows of that fault, and assert `PE_EVT_FAULT_CONFIRMED` reports `predicted_class == 5` and `class_confidence >= 70`.
     * `ZTEST(picoedge_suite, test_05_knowledge_and_centroid_serde_crc)`:
       * Export knowledge via `pe_export_knowledge(&ctx, buf, sizeof(buf))`.
       * Flip 1 bit (`buf[20] ^= 0x01`), assert `pe_import_knowledge(&ctx2, buf, len) == -EINVAL`.
       * Unflip bit, assert `pe_import_knowledge(&ctx2, buf, len) == 0`, and verify `ctx2` produces identical similarity scores on the next window.
       * Repeat round-trip test for `pe_export_centroid()` and `pe_import_centroid()`.
     * `ZTEST(picoedge_suite, test_06_hotload_pe_pkg_weights)`:
       * Call `pe_hotload_weights(&ctx, PE_SAMPLE_PKG_BLOB, sizeof(PE_SAMPLE_PKG_BLOB))` and assert return `0` and valid classification across all 4 classes.
* **Verification Gate:** `west build -b native_sim tests/ztest_picoedge -t run` passes 6/6 tests.

---

### WBS 5.2: nRF9161 DK Bench Harness (`samples/nrf9161dk_bench/`)
* **Task ID:** `WBS 5.2.1`
* **Target Files:** `samples/nrf9161dk_bench/CMakeLists.txt`, `samples/nrf9161dk_bench/prj.conf`, `samples/nrf9161dk_bench/src/main.c`
* **Dependencies:** `WBS 5.1.1`
* **Implementation Details:**
  1. Configure `prj.conf` with `CONFIG_PICOEDGE_AI=y`, `CONFIG_FPU=y`, `CONFIG_CMSIS_DSP=y`, `CONFIG_GPIO=y`, `CONFIG_FLASH=y`, `CONFIG_NVS=y`, `CONFIG_NETWORKING=n`.
  2. In `src/main.c`:
     * Initialize GPIO buttons `sw0..sw3` and LEDs `led0..led3` via Zephyr Devicetree (`DT_ALIAS(sw0)`, etc.) with graceful fallback if building on a target without aliases.
     * Run a 10 Hz periodic loop feeding windows into `pe_process_window()`, timing execution via `k_cycle_get_32()` and printing `[PicoEdge AI] Window processed in %u us | Sim: %u%% | Regime: %u | Class: %u`.
* **Verification Gate:** `west build -b nrf9161dk/nrf9161/ns samples/nrf9161dk_bench` succeeds with 0 warnings.

---

### WBS 5.3: Multi-Architecture & Memory Footprint Gate (`scripts/audit_footprint.py`)
* **Task ID:** `WBS 5.3.1`
* **Target Files:** `scripts/audit_footprint.py`
* **Dependencies:** `WBS 5.2.1`
* **Implementation Details:**
  Write a Python verification script that:
  1. Runs `arm-none-eabi-size` (or `size`) on `build/zephyr/libpicoedge_ai.a` from the `nrf9161dk/nrf9161/ns` build.
  2. Sums `.text` + `.rodata` + `.data` across `picoedge_dsp.c.obj`, `picoedge_core.c.obj`, `picoedge_serde.c.obj`, and `picoedge_smf.c.obj`.
  3. Asserts `total_flash_bytes <= 4096`.
  4. Compiles a 5-line C snippet printing `sizeof(struct pe_context)` and `sizeof(pe_diag_frame_t)`, asserting `sizeof(struct pe_context) <= 2048` and `sizeof(pe_diag_frame_t) == 12`.
  5. Optionally runs `west build -b qemu_riscv32 tests/ztest_picoedge -t run` to certify RISC-V `RV32IMFC` portability for V2 LM10xx hardware.
* **Verification Gate:** `python3 scripts/audit_footprint.py` prints the certified resource table and exits `0`.