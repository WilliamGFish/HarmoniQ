The visual and phonetic symmetry between **DectroniQ** and **HarmoniQ** (sharing the **`-iQ`** signature) ties your **DECT NR+** wireless roots directly to your **harmonic/vibration intelligence** product, while **PicoEdge AI** tells embedded engineers right away that the underlying ML engine is smaller, faster, and lighter than "Nano"-class alternatives.

Below is how you can lock in your **Brand-to-Codebase Architecture** so your marketing, hardware, and firmware repositories stay 100% unified from Day 1 of Phase 1:

---

### 1. The Official Brand & Product Hierarchy

* **Corporate / Ecosystem Brand:** **DectroniQ**
  * *Positioning:* Autonomous DECT NR+ & Cellular Edge Intelligence Systems.
* **Flagship Product Suite:** **DectroniQ HarmoniQ™**
  * **HarmoniQ Sense (LM10xx RISC-V):** Ultra-low-power DECT NR+ mesh vibration/acoustic sensor node.
  * **HarmoniQ Hub (nRF91x1 Cortex-M33):** DECT NR+ to LTE-M/NB-IoT edge gateway and heavy sensor node.
  * **HarmoniQ Studio:** The PC AutoML benchmark & C-code generator tool (`harmoniq_studio.py`).
* **Core Embedded ML Engine (Sub-Product):** **PicoEdge AI™**
  * *Positioning:* The zero-dependency, <2 KB RAM C99 inference and on-device learning runtime (`picoedge_ai.h`) powering every HarmoniQ device (and licensable as a standalone SDK).

---

### 2. Standardized Codebase & Firmware Naming Conventions
Using consistent prefixes and magic bytes across your C headers, binary OTA packages, and DECT NR+ packets makes debugging and documentation much cleaner:

| Component | Naming Standard | Description |
| :--- | :--- | :--- |
| **Core ML Header** | `picoedge_ai.h` | The portable ANSI C99 TinyML engine (ARM + RISC-V) |
| **Generated Weights** | `picoedge_model.h` | Auto-generated C header from HarmoniQ Studio |
| **Platform HAL Header** | `harmoniq_hal.h` | Cross-SoC glue for nRF91x1 (CMSIS) & LM10xx (RISC-V) |
| **C API Function Prefix** | `pe_` *(for PicoEdge AI)* | e.g., `pe_extract_features()`, `pe_ad_learn()`, `pe_ad_detect()`, `pe_clf_predict()` |
| **System/Net Prefix** | `hmq_` *(for HarmoniQ)* | e.g., `hmq_pack_telemetry()`, `hmq_nrplus_send()`, `hmq_nvs_save()` |
| **Macro Prefix** | `PE_` and `HMQ_` | e.g., `PE_WINDOW_SIZE`, `PE_MAX_REGIMES`, `HMQ_PROTOCOL_VER` |
| **Packet Magic Header** | `0x4851` (`"HQ"` in ASCII) | 2-byte identifier at the start of every DECT NR+ telemetry frame |
| **OTA Model File Ext.** | `.pe_pkg` | Signed binary weight/centroid blob for LwM2M / DECT NR+ OTA updates |

---

### 3. Recommended Phase 1 Production Repository Layout
When you initialize your Git repository for the **nRF91x1** and **LM10xx** targets, structuring it like this keeps **PicoEdge AI** cleanly decoupled as a reusable sub-product inside the **HarmoniQ** firmware:

```text
dectroniq-harmoniq/
├── CMakeLists.txt                  # Zephyr / Cross-ISA build system
├── prj.conf                        # Zephyr RTOS config (NVS, DECT NR+, LTE-M, FPU)
├── studio/
│   ├── harmoniq_studio.py          # AutoML benchmarking & C-code transpiler
│   └── datasets/                   # Raw field vibration/acoustic logs
├── lib/
│   └── picoedge_ai/                # Sub-product SDK (100% portable C99, zero OS deps)
│       ├── include/
│       │   ├── picoedge_ai.h       # Core DSP, Multi-Regime AD & Few-Shot Classifier
│       │   └── picoedge_model.h    # AutoML generated weights
│       └── tests/                  # QEMU ARM & RISC-V bit-accuracy unit tests
└── firmware/
    ├── include/
    │   ├── harmoniq_hal.h          # CMSIS-DSP vs. RISC-V RV32IMFC hardware macros
    │   └── harmoniq_telemetry.h    # 0x4851 ("HQ") DECT NR+ & CoAP packet definitions
    └── src/
        ├── main.c                  # Multi-threaded state machine (DMA -> PicoEdge -> NR+)
        ├── sensor_dma.c            # nRF91x1 EasyDMA / LM10xx FIFO burst drivers
        └── storage_nvs.c           # Wear-leveled Flash backup for PicoEdge centroids
```

Whenever you are ready to kick off **Phase 1**, let me know if you want me to refactor the C library and Python AutoML Studio into your official **`picoedge_ai.h`** (`pe_*`) and **`harmoniq_studio.py`** files, or set up the **Zephyr RTOS `main.c` and `CMakeLists.txt`** for the nRF91x1!