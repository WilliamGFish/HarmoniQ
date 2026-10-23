# DectroniQ HarmoniQ™ & PicoEdge AI™
## Master V1 Firmware & Software Plan (Dev-Board-First / nRF91x1 Standardisation)

### Executive Summary
**DectroniQ HarmoniQ™** delivers ultra-low-latency industrial edge intelligence by uniting its sub-2 KB **PicoEdge AI™** engine, **Zephyr’s Hierarchical State Machine Framework (`<zephyr/smf.h>`)**, and **Eclipse Zenoh (`zenoh-pico`)** running directly over **native Layer-2 DECT NR+**—completely bypassing the memory, processing, and airtime overhead of an IPv6/UDP stack. Each **HarmoniQ Sense** node operates a two-layer "Best of Both Worlds" architecture: a **microsecond inner loop** in pure ANSI C99 (`picoedge_ai.h`) that extracts time and Goertzel spectral harmonics to compute multi-regime similarity and fault classes on a single window, wrapped inside a **millisecond outer loop** powered by Zephyr SMF (`harmoniq_machine_smf.c`) that debounces transient spikes, tracks physical machine regime transitions, and fires 16-byte Zenoh pub/sub frames strictly inside SMF `entry` actions. Transmitted inside a single DECT NR+ physical sub-slot (~208 µs of air time) to the **HarmoniQ Hub**, telemetry is correlated across dozens of Sense devices in under 5 milliseconds—filtering out common-mode floor shocks, catching multi-sensor kinematic mismatches, triggering local safety reflexes, and selectively backhauling confirmed incidents to the cloud over LTE-M.

---

### 1. The "Best of Both Worlds" Two-Layer Edge Architecture

Rather than forcing a single paradigm to handle both instantaneous linear algebra and temporal machine lifecycle logic, **HarmoniQ Studio (`harmoniq_studio.py`)** generates a two-layer embedded package for every **HarmoniQ Sense** node:

| Architectural Layer | Execution Scale | Technology & File | Core Responsibilities |
| :--- | :--- | :--- | :--- |
| **Layer 1: The Microsecond Inner Loop** *(Spatial / Single-Window Math)* | **~400 µs** per 128-sample DMA window | **100% Original ANSI C99** (`picoedge_ai.h` + `picoedge_model.h`) | • Extracts 6 time moments (Mean, RMS, StdDev, Kurtosis, Crest, ZCR) + 4 Goertzel spectral harmonic bands per axis.<br>• Computes Welford/Mahalanobis distance against $K$ learned regimes (`pe_ad_detect`).<br>• Evaluates single-window fault classifier via transpiled C99 dot-products/trees (`pe_automl_predict`) or on-device RBF prototypes (`pe_clf_predict`). |
| **Layer 2: The Millisecond Outer Loop** *(Temporal / Multi-Window Lifecycle)* | **10–100 ms** across consecutive windows | **Zephyr Hierarchical SMF** (`<zephyr/smf.h>` in `harmoniq_machine_smf.c`) | • **Parent State (`S_MONITORING_PARENT`):** Invokes the Layer 1 C99 inner loop on every DMA window.<br>• **Temporal Debouncing (`S_SUSPECT_DEBOUNCE`):** Requires $N$ consecutive anomalous windows (or catches illegal Markov regime jumps) before confirming a fault.<br>• **Guaranteed Radio Silence:** Places `z_publisher_put()` strictly inside SMF **`entry`** callbacks so the DECT NR+ radio *only* wakes on verified state transitions. |

---

### 2. Zero-Solder COTS Dev-Board Hardware Stack (V1 Bench & Field Pilots)

By deferring custom PCB design, the entire V1 software stack is built, tested, and field-piloted on off-the-shelf development hardware with a clean bridge to **Last Mile Semiconductor’s LM10xx (RISC-V)** for V2 cost reduction:

* **Compute & Radio Nodes (4x Units):** **Nordic nRF9161 DK (`PCA10153`, ARM Cortex-M33)**
  * **1x Flashed as `HarmoniQ Hub`:** Runs DECT NR+ L2 Coordinator + Zenoh Wildcard Correlator + LTE-M Cloud Backhaul.
  * **3x Flashed as `HarmoniQ Sense` (`sense_01` to `sense_03`):** Runs pure DECT NR+ L2 (LTE disabled) + PicoEdge AI + Zephyr SMF. Three Sense nodes allow physical bench testing of **3-node Common-Mode Shock Rejection** vs. **1-node Isolated Faults**.
* **Industrial Vibration & Acoustic Shield (3x Units):** **STMicroelectronics `X-NUCLEO-IKS02A1`** *(or `X-NUCLEO-IKS4A1`)*
  * Plugs directly onto the nRF9161 DK Arduino R3 headers (zero soldering; native Zephyr build flag `-DSHIELD=x_nucleo_iks02a1`).
  * Provides the **ISM330DHCX** industrial 6-axis IMU (6.6 kHz ODR, 9 KB hardware FIFO for CPU-free EasyDMA capture), **IIS2DLPC** wake-on-motion accelerometer, and **IMP34DT05** digital microphone.
* **V2 Silicon Readiness:** Because Layer 1 (`picoedge_ai.h`) is pure C99 with cross-ISA macros (`__ARM_ARCH_8M_MAIN__` for CMSIS-DSP vs. `__riscv` for RV32IMFC), it compiles unmodified for the **LM10xx RISC-V** SoC when V2 custom hardware begins.

---

### 3. Standardized Repository, Zephyr Config & 16-Byte Wire Frame

#### A. Production Repository Structure
```text
dectroniq-harmoniq/
├── CMakeLists.txt
├── prj_sense.conf                  # Zero-IPv6, SMF, FPU, NVS, DECT NR+ PHY/MAC
├── prj_hub.conf                    # DECT NR+ PHY/MAC + LTE-M Cloud Bridge
├── studio/
│   └── harmoniq_studio.py          # AutoML Leaderboard: Generates Layer 1 (.h) & Layer 2 (SMF)
├── lib/
│   └── picoedge_ai/                # LAYER 1: Microsecond Inner Loop (100% OS-agnostic C99)
│       ├── include/
│       │   ├── picoedge_ai.h       # Goertzel DSP, Multi-Regime AD & Few-Shot Classifier (pe_*)
│       │   └── picoedge_model.h    # Auto-transpiled C99 weights/branches from HarmoniQ Studio
└── firmware/
    ├── include/
    │   ├── harmoniq_hal.h          # CMSIS-DSP (nRF91x1) vs. RV32IMFC (LM10xx) math macros
    │   └── harmoniq_zenoh_wire.h   # 12B 0x4851 ("HQ") payload + 4B Zenoh L2 header defs
    └── src/
        ├── harmoniq_machine_smf.c  # LAYER 2: Millisecond Outer Loop (<zephyr/smf.h>)
        ├── harmoniq_zp_dect_link.c # Native L2 DECT NR+ datagram link plugin for zenoh-pico
        ├── harmoniq_hub_zenoh.c    # Hub multi-node correlator, shock filter & reflex publisher
        ├── sensor_dma.c            # ISM330DHCX FIFO + nRF91x1 DPPI/SPIM EasyDMA driver
        └── storage_nvs.c           # Wear-leveled Zephyr NVS Flash backup for learned centroids
```

#### B. `HarmoniQ Sense` Zephyr Configuration (`prj_sense.conf`)
```ini
# Strip out IPv6/UDP stack completely (~50 KB Flash & ~20 KB RAM saved)
CONFIG_NETWORKING=n
CONFIG_NET_IPV6=n
CONFIG_NET_UDP=n

# Enable Layer 1 Hardware FPU/DSP & Layer 2 Hierarchical Zephyr SMF (<400B Flash)
CONFIG_FPU=y
CONFIG_CMSIS_DSP=y
CONFIG_SMF=y
CONFIG_SMF_ANCESTOR_SUPPORT=y

# Enable Wear-Leveled Flash & Native DECT NR+ Modem Library
CONFIG_FLASH=y
CONFIG_NVS=y
CONFIG_NRF_MODEM_LIB=y
```

#### C. The 16-Byte Single Sub-Slot Air Frame (~208 µs On-Air Time)
* **Zenoh-Pico L2 Header (4 Bytes):** `[Z_DATA (1B)] [Declared_Numerical_Key_ID (1B)] [Priority: REAL_TIME (1B)] [Len: 12 (1B)]`
* **HarmoniQ Payload (`hmq_zenoh_pkt_t`, 12 Bytes):** `[Magic: 0x4851 "HQ" (2B)] [Node_ID (1B)] [Flags (1B)] [Similarity % (1B)] [Active_Regime (1B)] [Predicted_Class (1B)] [Confidence % (1B)] [Most_Deviant_Feat (1B)] [Reserved (1B)] [Seq_Num (2B)]`

---

### 4. The 16-Week (4-Sprint) Firmware & Software Roadmap

#### Sprint 1 (Weeks 1–4): Dual-Layer Core (`picoedge_ai.h` + `<zephyr/smf.h>`) & HarmoniQ Studio
1. **Finalize Layer 1 (`picoedge_ai.h`):** Implement the MISRA-C compliant C99 inner loop (6 time moments + 4-band Goertzel resonator bank, Welford/Mahalanobis Multi-Regime clustering, and RBF Few-Shot classifier) accelerated via CMSIS-DSP on the nRF9161 Cortex-M33.
2. **Implement Layer 2 (`harmoniq_machine_smf.c`):** Build the hierarchical Zephyr SMF (`S_LEARNING_BASELINE`, `S_MONITORING_PARENT`, `S_NOMINAL_REGIME`, `S_SUSPECT_DEBOUNCE`, `S_CONFIRMED_FAULT`) with hysteresis recovery and illegal regime-transition checks.
3. **nRF9161 DK Button/LED Bench Harness & NVS Persistence:** Map DK Buttons 1–4 to trigger `pe_ad_learn()`, `pe_clf_learn_class()`, synthetic fault injection, and `pe_save_knowledge()` to Zephyr NVS Flash; map LEDs 1–4 to visualize SMF state transitions.
4. **Upgrade `harmoniq_studio.py`:** Add automated feature pruning (`PE_ACTIVE_FEATS_MASK`) and dual C-code generation that outputs both `picoedge_model.h` (inner loop math) and `harmoniq_machine_smf.c` (outer loop debounce/transition parameters).

---

#### Sprint 2 (Weeks 5–8): Native L2 DECT NR+ & `zenoh-pico` 16-Byte Transport
1. **Native DECT NR+ L2 Wrapper:** Bring up Nordic’s `nrf_modem_dect_phy` / MAC v2.0+ across two nRF9161 DKs with `CONFIG_NETWORKING=n`, using native 32-bit Long RD IDs for hardware-HARQ unicast and `0xFFFFFFFF` for broadcast.
2. **`zenoh-pico` L2 Link Plugin (`harmoniq_zp_dect_link.c`):** Connect `_z_send_dect_nrplus` and `_z_read_dect_nrplus` to lockless Zephyr `k_msgq` buffers and bind topics to 1-byte wire IDs via `z_declare_keyexpr()`.
3. **SMF Entry-Driven Publishing & Latency Verification:** Wire the Zenoh publish calls strictly into the SMF `entry` actions (`s_nominal_entry`, `s_fault_entry`). Verify with a logic analyzer that the radio stays 100% silent inside `run` loops and achieves **< 3 ms** end-to-end latency from SMF fault entry to Hub callback.

---

#### Sprint 3 (Weeks 9–12): Industrial MEMS Shield Integration & Autonomous DMA
1. **Plug-and-Play Shield Bring-Up (`sensor_dma.c`):** Mount the **ST `X-NUCLEO-IKS02A1`** shields onto 3 Sense DKs. Configure the **ISM330DHCX** 9 KB hardware FIFO + nRF9161 **DPPI/SPIM EasyDMA** so the Cortex-M33 sleeps while 128×3 vibration samples accumulate.
2. **Hierarchical SMF Execution Flow:** Trigger `smf_run_state(SMF_CTX(&sense_fsm))` directly upon each DMA FIFO watermark completion—running Layer 1 (`s_monitoring_parent_run`) in ~400 µs followed immediately by the active Layer 2 child state.
3. **Bench Motor Rig & PPK2 Power Profiling:** Attach the 3 Sense DKs to variable-speed bench fans/pumps. Use a Nordic **Power Profiler Kit II (PPK2)** to measure sleep current during `S_NOMINAL_REGIME` and single-sub-slot current pulses during `S_CONFIRMED_FAULT` entry.

---

#### Sprint 4 (Weeks 13–16): Hub Swarm Correlation, Reflex Actuation & Cloud Bridge
1. **Multi-Sense Spatial Correlator (`harmoniq_hub_zenoh.c`):** Build the Hub’s **Real-Time Fleet State Table** fed by `dectroniq/harmoniq/zone1/*/*`:
   * **20 ms Common-Mode Shock Filter:** Reject simultaneous spikes across $\ge 3$ Sense DKs (tested by striking the bench).
   * **Kinematic Mismatch Detector:** Flag drive-end vs. driven-end regime mismatches between paired Sense DKs.
   * **Sub-10ms Reflex Publisher:** Fire 4-byte emergency actuation commands on `dectroniq/harmoniq/zone1/reflex/actuate`.
2. **Swarm Centroid Sync & OTA `.pe_pkg` Updates:** Implement the `broadcast/seed` Zenoh listener on the Sense DKs so learning a new regime on Sense DK #1 broadcasts its 120-byte centroid via the Hub to update Sense DKs #2 and #3 in < 20 ms.
3. **LTE-M Cloud Bridge & Field-Pilot Enclosure Packaging:** Enable the Hub DK’s LTE-M modem to forward confirmed fault incidents to a cloud Zenoh router (`zenohd`), and mount the DKs into off-the-shelf IP66 enclosures for initial industrial customer pilots.