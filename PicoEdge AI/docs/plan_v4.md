# DectroniQ HarmoniQ™ — PicoEdge AI™ Library & Studio
## Master Agentic Implementation Plan & Work Breakdown Structure (WBS)

> **Purpose of This Document:** This specification is structured as an autonomous execution blueprint (`PLAN.md` / `SPEC.md`) for an **Agentic Coding Harness** (e.g., Antigravity CLI, Claude Code, Cursor Agent, OpenHands). Every phase is broken down into deterministic, atomic Work Breakdown Structure (WBS) tasks with explicit file paths, data contracts, mathematical formulas, and automated CLI verification gates.

---

### 1. Global Agentic Directives & Architectural Constraints

The autonomous coding agent MUST enforce the following invariants across all generated files:
1. **Strict Scope Boundary:** This repository builds **only** the drop-in **PicoEdge AI™ Zephyr Module (`lib/picoedge_ai`)** and **HarmoniQ Studio™ (`studio/`)**. It must contain **zero** radio, DECT NR+, or Zenoh dependencies. Communication with the host firmware happens strictly via `include/picoedge.h` callbacks and byte-array serialization functions.
2. **MISRA-C / Embedded Safety Rules:**
   * **Zero Dynamic Memory:** `malloc()`, `calloc()`, `realloc()`, and `free()` are strictly banned. All state resides inside the caller-allocated `struct pe_context`.
   * **Floating-Point Safety:** Every division must include `+ PE_EPSILON` (`1e-5f`) in the denominator. All `sqrtf()`, `log10f()`, and `expf()` inputs must be clamped to valid non-negative or bounded ranges to prevent `NaN` or `Inf` propagation.
   * **Layer Separation:** 
     * **Layer 1 (Inner Loop — `src/picoedge_dsp.c`, `src/picoedge_core.c`):** 100% OS-agnostic ANSI C99 (`<stdint.h>`, `<stddef.h>`, `<stdbool.h>`, `<string.h>`, `<math.h>`). Must compile cleanly under both `gcc`/`clang` (for Python `ctypes` parity tests) and Zephyr toolchains.
     * **Layer 2 (Outer Loop — `src/picoedge_smf.c`):** Zephyr Hierarchical State Machine (`<zephyr/smf.h>`) wrapping Layer 1 and firing host callbacks **exclusively inside SMF `entry` actions**.
3. **Hard Resource Budgets (Enforced in Phase 5):**
   * **RAM (`sizeof(struct pe_context)`):** $\le 2,048\text{ Bytes}$ (excluding caller’s raw sensor DMA buffer).
   * **Flash (`.text` + `.rodata` of `libpicoedge_ai.a`):** $\le 4,096\text{ Bytes}$ on `arm-none-eabi-gcc -Os`.
   * **Execution Time:** $\le 400\ \mu\text{s}$ per 128×3 window on a 64 MHz ARM Cortex-M33 (`nrf9161dk/nrf9161/ns`).

---

### 2. Target Repository File Tree

The agent shall construct the following exact directory structure:

```text
picoedge-ai/
├── zephyr/
│   └── module.yml                       # Zephyr external module definition
├── Kconfig                              # Zephyr Kconfig options (CONFIG_PICOEDGE_AI, etc.)
├── CMakeLists.txt                       # Zephyr library build rules
├── include/
│   ├── picoedge.h                       # Public Host Firmware API & 12-byte wire struct
│   ├── picoedge_core.h                  # Layer 1 OS-agnostic C99 internal structs & prototypes
│   ├── picoedge_hal.h                   # Cross-ISA DSP macros (CMSIS-DSP vs. RV32IMFC vs. C99)
│   └── picoedge_model.h                 # Default/Generated AutoML weights & SMF thresholds
├── src/
│   ├── picoedge_dsp.c                   # Layer 1: Single-pass 6 time moments + 4-band Goertzel
│   ├── picoedge_core.c                  # Layer 1: Welford, K-Regime Mahalanobis AD, RBF & AutoML
│   ├── picoedge_serde.c                 # Layer 1: CRC-16-CCITT & Knowledge/Centroid/Weight serde
│   └── picoedge_smf.c                   # Layer 2: Zephyr <zephyr/smf.h> outer loop & API entrypoints
├── studio/
│   ├── requirements.txt                 # numpy, scikit-learn, pytest
│   ├── harmoniq_dsp.py                  # Bit-accurate Python float32 mirror of picoedge_dsp.c
│   ├── harmoniq_studio.py               # AutoML leaderboard, RFE feature pruning & C/pkg transpiler
│   ├── generate_synthetic_data.py       # Synthetic multi-regime & bearing-fault CSV generator
│   └── tests/
│       └── test_studio_c_parity.py      # Compiles Layer 1 C99 via ctypes & asserts <1e-5 parity
├── tests/
│   └── ztest_picoedge/                  # Zephyr automated unit & SMF integration test suite
│       ├── CMakeLists.txt
│       ├── prj.conf
│       └── src/
│           └── main.c
├── samples/
│   └── nrf9161dk_bench/                 # Standalone nRF9161 DK bench harness (Buttons/LEDs/NVS)
│       ├── CMakeLists.txt
│       ├── prj.conf
│       └── src/
│           └── main.c
└── scripts/
    └── audit_footprint.py               # Parses .elf via size/nm to enforce <2KB RAM & <4KB Flash
```

---

### 3. Master Data Contracts & Mathematical Specifications

To prevent interface mismatches between WBS tasks, the agent must adhere to these exact binary and mathematical definitions:

#### 3.1 Constants & Kconfig Defaults
* `PE_WINDOW_SIZE` = `128` (Samples per channel per window)
* `PE_AXES` = `3` (Interleaved input buffer: `[x0, y0, z0, x1, y1, z1, ...]`)
* `PE_FEATS_PER_AXIS` = `10` (6 Time-Domain + 4 Goertzel Spectral Bands)
* `PE_NUM_FEATURES` = `(PE_AXES * PE_FEATS_PER_AXIS)` = `30`
* `PE_MAX_REGIMES` = `4` (Max dynamic healthy operating modes)
* `PE_MAX_CLASSES` = `6` (Max fault classes)
* `PE_MAGIC_HEADER` = `0x4851` (`"HQ"` in little-endian ASCII)
* `PE_KNOWLEDGE_MAGIC` = `0x50454B31` (`"PEK1"` for NVS/Swarm knowledge blobs)
* `PE_PKG_MAGIC` = `0x50455731` (`"PEW1"` for `.pe_pkg` OTA weight blobs)

#### 3.2 Feature Ordering Per Axis (`base = axis * 10`)
* `features[base + 0]`: **Mean** ($\mu = \frac{1}{N}\sum x_i$)
* `features[base + 1]`: **RMS** ($\sqrt{\frac{1}{N}\sum x_i^2}$)
* `features[base + 2]`: **StdDev** ($\sigma = \sqrt{\frac{1}{N}\sum (x_i - \mu)^2}$)
* `features[base + 3]`: **Kurtosis** ($\frac{\frac{1}{N}\sum (x_i - \mu)^4}{(\sigma^2)^2 + \epsilon}$)
* `features[base + 4]`: **Crest Factor** ($\frac{\max |x_i|}{\text{RMS} + \epsilon}$)
* `features[base + 5]`: **Zero-Crossing Rate (ZCR)** around $\mu$ ($\frac{\text{sign flips}}{N}$)
* `features[base + 6..9]`: **4-Band Goertzel Log Power** at normalized frequencies $f/f_s \in \{0.05, 0.12, 0.25, 0.40\}$ with coefficients $C_b = 2\cos(2\pi f_b/f_s) \in \{1.902113\text{f}, 1.457937\text{f}, 0.0\text{f}, -1.618034\text{f}\}$:
  * Single-pass recursion on $d_i = x_i - \mu$: $s_0 = d_i + C_b s_1 - s_2$; $s_2 = s_1$; $s_1 = s_0$.
  * Power: $P_b = s_1^2 + s_2^2 - C_b s_1 s_2$.
  * Feature value: $\log_{10}\left(\frac{|P_b|}{N} + 1.0\text{f}\right)$.

---

### 4. Phased Work Breakdown Structure (WBS)

---

#### PHASE 1: Repository Scaffolding, Build System & Public Contracts
**Objective:** Establish the Zephyr module infrastructure, Kconfig parameters, and header contracts so empty stub builds succeed before writing algorithmic logic.

* **WBS 1.1: Initialize Repository & Zephyr Module Manifest**
  * **Target Files:** `zephyr/module.yml`, `.gitignore`
  * **Implementation Spec:** Create `zephyr/module.yml` declaring `name: picoedge_ai`, `build: cmake: .` and `kconfig: Kconfig`.
  * **Verification Gate:** Directory structure exists and YAML syntax validates.

* **WBS 1.2: Author Parametric `Kconfig`**
  * **Target Files:** `Kconfig`
  * **Implementation Spec:** Define `config PICOEDGE_AI` (bool, selects `SMF` and `SMF_ANCESTOR_SUPPORT`). Under `if PICOEDGE_AI`, define integer configs with defaults:
    * `CONFIG_PE_WINDOW_SIZE` (default `128`)
    * `CONFIG_PE_AXES` (default `3`)
    * `CONFIG_PE_MAX_REGIMES` (default `4`)
    * `CONFIG_PE_MAX_CLASSES` (default `6`)
    * `CONFIG_PE_MIN_LEARN_WINDOWS` (default `25`)
    * `CONFIG_PE_ANOMALY_THRESH_PCT` (default `75`)
    * `CONFIG_PE_HYSTERESIS_CLEAR_PCT` (default `85`)
    * `CONFIG_PE_DEBOUNCE_WINDOWS` (default `3`)
  * **Verification Gate:** Kconfig parses cleanly via Zephyr `west build`.

* **WBS 1.3: Author `CMakeLists.txt` Build Configuration**
  * **Target Files:** `CMakeLists.txt`
  * **Implementation Spec:** Use `zephyr_library_named(picoedge_ai)`, `zephyr_include_directories(include)`, and `zephyr_library_sources(src/picoedge_dsp.c src/picoedge_core.c src/picoedge_serde.c src/picoedge_smf.c)` guarded by `if(CONFIG_PICOEDGE_AI)`. Enable `-O2` or `-Os` optimization flags for library sources.

* **WBS 1.4: Define Public & Core Header Contracts**
  * **Target Files:** `include/picoedge.h`, `include/picoedge_core.h`, `include/picoedge_hal.h`, `include/picoedge_model.h`
  * **Implementation Spec:**
    * In `include/picoedge.h`: Define `pe_event_type_t`, `pe_op_mode_t`, the 12-byte packed `pe_diag_frame_t` (`_Static_assert(sizeof(pe_diag_frame_t) == 12, "Frame must be 12 bytes")`), `struct pe_context` (opaque/complete definition so caller can statically allocate it), and all public functions (`pe_init`, `pe_set_mode`, `pe_process_window`, `pe_export_knowledge`, `pe_import_knowledge`, `pe_export_centroid`, `pe_import_centroid`, `pe_hotload_weights`).
    * In `include/picoedge_core.h`: Define the OS-agnostic Layer 1 structs (`pe_ad_engine_t`, `pe_clf_engine_t`, `pe_runtime_weights_t`) and pure C99 function prototypes so Layer 1 can be compiled without Zephyr headers during Python `ctypes` testing.
  * **Verification Gate:** Compile a dummy C file `#include "picoedge_core.h"` with host `gcc -std=c99 -Wall -Wextra -Werror -pedantic`.

---

#### PHASE 2: Layer 1 — Pure ANSI C99 Microsecond Inner Loop
**Objective:** Implement the single-window DSP feature extractor, Multi-Regime Anomaly Detector, and Dual-Mode Classifier in pure, portable C99.

* **WBS 2.1: Cross-ISA Hardware DSP Abstraction Layer**
  * **Target Files:** `include/picoedge_hal.h`
  * **Implementation Spec:** Implement `pe_hal_dot_prod_f32(const float *a, const float *b, uint32_t len)` and `pe_hal_mean_rms_min_max_f32(...)`:
    * Branch 1 (`#if defined(CONFIG_CMSIS_DSP) && defined(__ARM_ARCH_8M_MAIN__)`): Call `<arm_math.h>` `arm_dot_prod_f32()`.
    * Branch 2 (`#elif defined(__riscv)`): 4x loop-unrolled single-pass FMA accumulation optimized for `RV32IMFC`.
    * Branch 3 (`#else`): Standard ANSI C99 portable loop.
  * **Verification Gate:** Compiles cleanly under `gcc -std=c99` and `arm-none-eabi-gcc`.

* **WBS 2.2: Single-Pass Hybrid Time + Goertzel Feature Extractor**
  * **Target Files:** `src/picoedge_dsp.c`
  * **Implementation Spec:** Implement `void pe_extract_features_masked(const float *raw_window, float *out_feats, uint32_t active_feats_mask)`:
    * Compute all 10 features per axis per Section 3.2.
    * If a bit `b` in `active_feats_mask` is `0`, set `out_feats[b] = 0.0f` and skip unnecessary Goertzel bands or moment divisions if the entire group is masked off.
    * Guard all divisions with `+ PE_EPSILON` and clamp `variance >= 0.0f`.
  * **Verification Gate:** Feed a constant array and a pure sine wave in a small host test; verify RMS of $A\sin(\omega t)$ is $A/\sqrt{2}$, Kurtosis is $\approx 1.5$, and the matching Goertzel band peaks.

* **WBS 2.3: Multi-Regime Unsupervised Anomaly Detection Engine**
  * **Target Files:** `src/picoedge_core.c` (Part A)
  * **Implementation Spec:** Implement:
    * `pe_core_ad_init(pe_ad_engine_t *eng, uint32_t min_samples, float sensitivity)`
    * `pe_core_ad_learn(pe_ad_engine_t *eng, const float *feats, uint32_t mask)`:
      * Update `global_mean[i]` and `global_m2[i]` via Welford’s algorithm.
      * Compute normalized diagonal Mahalanobis distance $D(f, c_r) = \sqrt{\frac{1}{|M|}\sum_{i \in M} \frac{(f_i - c_{r,i})^2}{\sigma_i^2 + \epsilon}}$ against active regimes $r \in [0, K-1]$.
      * If `active_regimes == 0`, initialize Regime 0. Else if $\min_r D(f, c_r) > 2.5\text{f}$ and `active_regimes < PE_MAX_REGIMES` and `total_samples > 3`, spawn a new regime centroid. Else update the nearest centroid $c_{r^*}$ via running average.
    * `pe_core_ad_detect(const pe_ad_engine_t *eng, const float *feats, uint32_t mask, float *similarity_pct, uint8_t *matched_regime, uint8_t *most_deviant_feat)`:
      * Find nearest regime $r^*$ with minimum distance $D_{\min}$.
      * Identify `most_deviant_feat` as the unmasked feature index $i$ with the highest single-feature Z-score $\frac{(f_i - c_{r^*,i})^2}{\sigma_i^2 + \epsilon}$.
      * Compute `excess = (D_min > 1.3f) ? (D_min - 1.3f) : 0.0f` and `*similarity_pct = 100.0f * expf(-0.4f * eng->sensitivity * excess)`.
  * **Verification Gate:** Unit test verifies that learning 2 distinct synthetic speeds creates `active_regimes == 2`, both score `>95%` similarity, and an out-of-distribution spike scores `<50%` while pinpointing the exact spiked feature index.

* **WBS 2.4: Dual-Mode Fault Classifier (On-Device RBF + AutoML Static/Hotload)**
  * **Target Files:** `src/picoedge_core.c` (Part B), `include/picoedge_model.h`
  * **Implementation Spec:**
    * Implement On-Device Few-Shot RBF learner (`pe_core_clf_learn(pe_clf_engine_t *clf, const float *feats, uint8_t class_id)`) and predictor (`pe_core_clf_predict(...)`).
    * Implement AutoML predictor (`pe_core_automl_predict(...)`) that checks if runtime hot-loaded weights (`ctx->has_hotloaded_weights`) are active; if so, evaluates the linear/MLP matrix in `ctx->hot_weights`; otherwise calls the compile-time transpiled `pe_generated_model_predict(feats)` in `include/picoedge_model.h`.
    * Arbitrate between On-Device Few-Shot and AutoML: if a class was learned in-situ and its RBF confidence exceeds the static model’s confidence, return the in-situ class; otherwise return the AutoML prediction.
  * **Verification Gate:** Test both compile-time `picoedge_model.h` inference and runtime `pe_core_clf_learn()` class separation on synthetic feature vectors.

* **WBS 2.5: CRC-16-CCITT & Safe Binary Serialization (`src/picoedge_serde.c`)**
  * **Target Files:** `src/picoedge_serde.c`
  * **Implementation Spec:**
    * Implement table-less `pe_crc16_ccitt(const uint8_t *data, size_t len)` (polynomial `0x1021`, init `0xFFFF`).
    * Implement `pe_export_knowledge()` & `pe_import_knowledge()`: Serializes/deserializes `pe_ad_engine_t` + `pe_clf_engine_t` prefixed with magic `0x50454B31`, version `1`, payload length, and trailing `uint16_t crc16`. Reject imports on magic, length, or CRC mismatch without modifying `ctx`.
    * Implement `pe_export_centroid()` & `pe_import_centroid()`: Exports/imports a single 128-byte Regime or Fault Centroid struct (for 1-packet Zenoh Swarm Seed broadcasting).
    * Implement `pe_hotload_weights()`: Validates `.pe_pkg` magic `0x50455731` and CRC-16, then copies new scaler/linear/MLP weights into `ctx->hot_weights`.
  * **Verification Gate:** Round-trip export/import test; verify flipping 1 bit in the byte buffer returns `-EINVAL` and leaves `ctx` untouched.

---

#### PHASE 3: Layer 2 — Zephyr SMF Millisecond Outer Loop & Host Integration
**Objective:** Wrap Layer 1 inside Zephyr’s Hierarchical State Machine (`<zephyr/smf.h>`) to handle multi-window debouncing, hysteresis recovery, and strict entry-only callback firing.

* **WBS 3.1: Hierarchical Zephyr SMF State Table (`src/picoedge_smf.c`)**
  * **Target Files:** `src/picoedge_smf.c`
  * **Implementation Spec:** Define `static const struct smf_state pe_smf_states[]` using `SMF_CREATE_STATE`:
    * `PE_STATE_LEARNING` (Root state when unprimed):
      * `run`: Extracts features and calls `pe_core_ad_learn()`. When `PE_READY` is returned, transitions to `PE_STATE_NOMINAL` and sets `just_primed = true`.
    * `PE_STATE_MONITOR_PARENT` (Parent state for all monitoring sub-states):
      * `run`: Executes Layer 1 (`pe_extract_features_masked`, `pe_core_ad_detect`, and classifier) once per `pe_process_window()` call.
    * `PE_STATE_NOMINAL` (Child of `PE_STATE_MONITOR_PARENT`):
      * `entry`: Resets `anomaly_streak = 0` and `recovery_streak = 0`. Fires host callback with `PE_EVT_BASELINE_READY` (if `just_primed`), `PE_EVT_FAULT_CLEARED` (if returning from `PE_STATE_FAULT`), or `PE_EVT_REGIME_SHIFT`.
      * `run`: If `similarity_pct < CONFIG_PE_ANOMALY_THRESH_PCT`, transitions to `PE_STATE_SUSPECT`. Else if `active_regime != prev_regime`, updates `prev_regime` and fires `PE_EVT_REGIME_SHIFT`.
    * `PE_STATE_SUSPECT` (Child of `PE_STATE_MONITOR_PARENT`):
      * `run`: If `similarity_pct < CONFIG_PE_ANOMALY_THRESH_PCT`, increments `anomaly_streak`. If `anomaly_streak >= CONFIG_PE_DEBOUNCE_WINDOWS`, transitions to `PE_STATE_FAULT`. Else (if similarity recovered), resets `anomaly_streak = 0` and transitions silently back to `PE_STATE_NOMINAL` (suppressing the `entry` callback via a `silent_return` flag so glitches never wake the radio!).
    * `PE_STATE_FAULT` (Child of `PE_STATE_MONITOR_PARENT`):
      * `entry`: Increments `seq_num`, packs the 12-byte `pe_diag_frame_t` (`magic = 0x4851`), and fires host callback `PE_EVT_FAULT_CONFIRMED`.
      * `run`: If `similarity_pct >= CONFIG_PE_HYSTERESIS_CLEAR_PCT`, increments `recovery_streak`. When `recovery_streak >= CONFIG_PE_DEBOUNCE_WINDOWS`, transitions back to `PE_STATE_NOMINAL`. Else resets `recovery_streak = 0`.
  * **Verification Gate:** Trace state transitions across simulated window sequences.

* **WBS 3.2: Public API Entrypoints & Mode Switching**
  * **Target Files:** `src/picoedge_smf.c`
  * **Implementation Spec:** Implement `pe_init()`, `pe_set_mode()`, and `pe_process_window()`:
    * When `pe_set_mode(ctx, PE_MODE_LEARN_FAULT_CLASS, class_id)` is active, `pe_process_window()` extracts features and feeds `pe_core_clf_learn()` directly without disrupting the anomaly baseline.
    * When `pe_set_mode(ctx, PE_MODE_LEARN_NORMAL, 0)` is active, forces SMF state to `PE_STATE_LEARNING` to learn an additional operating regime.

---

#### PHASE 4: HarmoniQ Studio™ AutoML & Dual-Layer Code Transpiler
**Objective:** Build the Python AutoML desktop/CI suite (`studio/`) and verify bit-exact parity between Python training and C99 inference.

* **WBS 4.1: Bit-Accurate Python DSP Extractor (`studio/harmoniq_dsp.py`)**
  * **Target Files:** `studio/harmoniq_dsp.py`, `studio/requirements.txt`
  * **Implementation Spec:** Implement `extract_features_py(raw_window, active_mask=0x3FFFFFFF)` using explicit `np.float32` arithmetic matching `src/picoedge_dsp.c` (including `PE_EPSILON = 1e-5` and the exact 4 Goertzel coefficients).

* **WBS 4.2: Automated Python-to-C99 `ctypes` Parity Test (`studio/tests/test_studio_c_parity.py`)**
  * **Target Files:** `studio/tests/test_studio_c_parity.py`
  * **Implementation Spec:**
    * In `pytest`, automatically invoke `gcc -shared -fPIC -O2 -std=c99 src/picoedge_dsp.c src/picoedge_core.c -Iinclude -lm -o /tmp/libpicoedge_test.so`.
    * Load `/tmp/libpicoedge_test.so` via Python `ctypes`.
    * Generate 50 random and harmonic `(128, 3)` `float32` windows, run both `extract_features_py()` and C `pe_extract_features_masked()`, and assert `np.max(np.abs(py_feats - c_feats)) < 1e-5`.
  * **Verification Gate:** `pytest studio/tests/test_studio_c_parity.py` must pass 100%.

* **WBS 4.3: Synthetic Multi-Regime & Fault Dataset Generator (`studio/generate_synthetic_data.py`)**
  * **Target Files:** `studio/generate_synthetic_data.py`
  * **Implementation Spec:** Generate reproducible labeled datasets (`.npz` / `.csv`) containing 4 classes across 3 regimes:
    * `Class 0`: Idle noise floor
    * `Class 1`: Healthy running (Regime 1 @ 5% $f_s$ and Regime 2 @ 12% $f_s$)
    * `Class 2`: Shaft Unbalance / Misalignment (elevated RMS + low-band Goertzel harmonics)
    * `Class 3`: Bearing Spall Impacts (periodic high-amplitude impulses driving high Kurtosis, Crest Factor, and Band 3 Goertzel energy).

* **WBS 4.4: AutoML Leaderboard, RFE Feature Pruning & Dual Transpiler (`studio/harmoniq_studio.py`)**
  * **Target Files:** `studio/harmoniq_studio.py`
  * **Implementation Spec:**
    * Run Recursive Feature Elimination (`RFE`) to find the optimal feature bitmask (`PE_ACTIVE_FEATS_MASK`) that retains $\ge 98\%$ of full-feature cross-validation accuracy.
    * Benchmark 4 architectures via 5-fold `StratifiedKFold`: `DecisionTreeClassifier(max_depth=4)`, `LinearSVC()`, `NearestCentroid()`, and `MLPClassifier(hidden_layer_sizes=(8,))`.
    * Compute exact Flash and RAM byte costs for each candidate and select the Pareto winner.
    * **Output 1 (`include/picoedge_model.h`):** Transpile the winning model, `PE_ACTIVE_FEATS_MASK`, feature scaler arrays, and recommended SMF debounce/threshold constants into pure C99.
    * **Output 2 (`model_weights.pe_pkg`):** Pack the scaler + linear/MLP weights into the binary `.pe_pkg` format (`0x50455731` + CRC-16) for runtime hot-loading tests.
  * **Verification Gate:** Run `python3 studio/harmoniq_studio.py --generate-default` and verify `include/picoedge_model.h` is updated and achieves $>97\%$ CV accuracy on the synthetic dataset.

---

#### PHASE 5: Automated Zephyr `ztest` Suite, Bench Sample & Footprint Audit
**Objective:** Prove the complete module inside Zephyr RTOS across POSIX simulation, ARM Cortex-M33 (`nrf9161dk`), and RISC-V, and enforce memory/latency budgets.

* **WBS 5.1: Comprehensive Zephyr `ztest` Suite (`tests/ztest_picoedge/`)**
  * **Target Files:** `tests/ztest_picoedge/CMakeLists.txt`, `tests/ztest_picoedge/prj.conf`, `tests/ztest_picoedge/src/main.c`
  * **Implementation Spec:** Implement 6 automated `ZTEST` cases:
    1. `test_in_situ_multi_regime_learning`: Feed 15 windows of Regime A (Idle) and 15 windows of Regime B (12% $f_s$ sine). Assert `PE_EVT_BASELINE_READY` fires once, `active_regimes == 2`, and switching between Regime A and B fires `PE_EVT_REGIME_SHIFT` with `similarity_pct >= 90%`.
    2. `test_single_window_glitch_debounce`: While in `PE_STATE_NOMINAL`, inject 2 consecutive bearing-fault windows (`< CONFIG_PE_DEBOUNCE_WINDOWS`) followed by healthy windows. Assert **zero** callbacks are fired (proving radio silence on transient bumps).
    3. `test_sustained_fault_and_hysteresis_recovery`: Inject 3 consecutive bearing-fault windows. Assert `PE_EVT_FAULT_CONFIRMED` fires on window #3 (and *not* again on window #4 or #5!), `frame.magic == 0x4851`, `frame.predicted_class == 3`, and `frame.most_deviant_feat` points to Kurtosis or Band 3. Then feed healthy windows and assert `PE_EVT_FAULT_CLEARED` fires after debounce recovery.
    4. `test_on_device_few_shot_learning`: Switch to `PE_MODE_LEARN_FAULT_CLASS` for Class 5, feed 5 windows of a novel 25% $f_s$ resonance, return to `PE_MODE_MONITOR`, re-inject the resonance, and assert `predicted_class == 5`.
    5. `test_knowledge_and_centroid_serde_crc`: Export knowledge via `pe_export_knowledge()`, corrupt 1 byte, assert `pe_import_knowledge()` returns `-EINVAL`, restore the byte, import into a blank `pe_context`, and assert bit-identical `similarity_pct`.
    6. `test_hotload_pe_pkg_weights`: Load the `.pe_pkg` binary blob via `pe_hotload_weights()` and verify live classification switches to the new weights.
  * **Verification Gate:** Execute `west build -b native_sim tests/ztest_picoedge -t run` (or `qemu_cortex_m3`) and assert `PROJECT EXECUTION SUCCESSFUL` with 6/6 tests passing.

* **WBS 5.2: nRF9161 DK Standalone Bench Harness (`samples/nrf9161dk_bench/`)**
  * **Target Files:** `samples/nrf9161dk_bench/CMakeLists.txt`, `samples/nrf9161dk_bench/prj.conf`, `samples/nrf9161dk_bench/src/main.c`
  * **Implementation Spec:**
    * Wire DK Buttons 1–4 (`sw0`..`sw3`) to: Learn Normal, Teach Fault Class, Inject Synthetic Bearing Fault, and Save Knowledge to Zephyr NVS Flash.
    * Wire DK LEDs 1–4 (`led0`..`led3`) to: Baseline Ready, Event Callback Fired (100ms pulse), Confirmed Fault Active, and Active Regime blink.
    * Measure and print exact cycle count and microseconds of `pe_process_window()` using ARM DWT cycle counter (`k_cycle_get_32()`).
  * **Verification Gate:** `west build -b nrf9161dk/nrf9161/ns samples/nrf9161dk_bench` compiles with zero warnings.

* **WBS 5.3: Automated Memory & Cross-ISA Audit Script (`scripts/audit_footprint.py`)**
  * **Target Files:** `scripts/audit_footprint.py`
  * **Implementation Spec:**
    * Inspect `libpicoedge_ai.a` from the `nrf9161dk/nrf9161/ns` build using `arm-none-eabi-size`.
    * Assert total Flash (`text + data`) $\le 4,096\text{ Bytes}$.
    * Assert `sizeof(struct pe_context)` $\le 2,048\text{ Bytes}$.
    * Run a cross-compile check on `qemu_riscv32` to verify RISC-V `RV32IMFC` compatibility for V2 LM10xx readiness.
  * **Verification Gate:** `python3 scripts/audit_footprint.py` exits with code `0` and prints the final certified RAM/Flash summary table.

---

### 5. Execution Order for the Agentic Harness

When handing this document to your coding agent, instruct it to execute the loop in this strict order:
1. **Execute Phase 1 (`WBS 1.1 – 1.4`)** $\rightarrow$ Verify header compilation.
2. **Execute Phase 2 (`WBS 2.1 – 2.5`)** $\rightarrow$ Build Layer 1 C99 files.
3. **Execute Phase 4 (`WBS 4.1 – 4.4`)** $\rightarrow$ Run `pytest studio/tests/test_studio_c_parity.py` and generate `include/picoedge_model.h` via `harmoniq_studio.py`.
4. **Execute Phase 3 (`WBS 3.1 – 3.2`)** $\rightarrow$ Build Layer 2 Zephyr SMF wrapper.
5. **Execute Phase 5 (`WBS 5.1 – 5.3`)** $\rightarrow$ Run Zephyr `ztest`, build `nrf9161dk_bench`, and pass `scripts/audit_footprint.py`.