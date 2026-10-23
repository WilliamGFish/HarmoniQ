# DectroniQ HarmoniQ™ & PicoEdge AI™
## Master V1 Production Plan (nRF91x1 Standardisation with LM10xx V2 Readiness)

### Executive Summary
**DectroniQ HarmoniQ™** delivers ultra-low-latency industrial edge intelligence by uniting its sub-2 KB **PicoEdge AI™** engine with **Eclipse Zenoh (`zenoh-pico`)** running directly over **native Layer-2 DECT NR+**, completely bypassing the memory, processing, and airtime overhead of a traditional IPv6/UDP stack. On each **HarmoniQ Sense** node, PicoEdge AI continuously learns and analyses time- and frequency-domain machine harmonics locally—remaining radio-silent during normal operation and distilling regime shifts or anomalies into a compact 12-byte diagnostic payload (`0x4851` "HQ"). Bound to a 1-byte Zenoh numerical key ID with just 4 bytes of protocol framing, the entire 16-byte pub/sub message transmits inside a single DECT NR+ physical sub-slot (~208 µs of air time) straight to the **HarmoniQ Hub**. Through zero-copy Zenoh wildcard subscriptions, the Hub correlates telemetry across dozens of Sense devices in under 5 milliseconds—enabling it to automatically filter out common-mode environmental shocks, verify isolated mechanical faults or multi-point kinematic mismatches, trigger immediate local safety reflexes, and selectively backhaul confirmed incidents to the cloud over LTE-M.

---

### 1. Product Ecosystem & Hardware Rollout Strategy

To eliminate silicon supply-chain risk and ship a commercial **V1 product within 24 weeks**, both the **HarmoniQ Sense V1** and **HarmoniQ Hub V1** standardize on Nordic Semiconductor’s mass-produced **nRF91x1 (ARM Cortex-M33)** architecture, while keeping the C99 codebase strictly ISA-agnostic so **Last Mile Semiconductor’s LM10xx (RISC-V)** can drop in as a cost-reduced **HarmoniQ Sense V2** node.

| Product Component | Role in Ecosystem | V1 Target Hardware | V2 Target Hardware | Key Software & Protocol Stack |
| :--- | :--- | :--- | :--- | :--- |
| **HarmoniQ Sense™** | Autonomous vibration/acoustic edge sensor node | **Nordic nRF9131 / nRF9161** *(DECT NR+ only; LTE disabled)* | **Last Mile Semi LM10xx** *(22nm FDSOI RISC-V)* | Zephyr RTOS (No IPv6), **PicoEdge AI**, `zenoh-pico` Native L2 DECT NR+ Publisher |
| **HarmoniQ Hub™** | Local swarm correlator, reflex controller & cloud gateway | **Nordic nRF9161 SiP** *(DECT NR+ & LTE-M active)* | **Nordic nRF9161 SiP** | Zephyr RTOS, `zenoh-pico` Wildcard Correlator, DECT NR+ L2 $\leftrightarrow$ LTE-M Cloud Bridge |
| **HarmoniQ Studio™** | Desktop AutoML benchmark & C99 code generator | PC / CI Pipeline (`harmoniq_studio.py`) | PC / CI Pipeline | Feature pruning, 5-fold CV leaderboard, exports `picoedge_model.h` & `.pe_pkg` OTA blobs |
| **PicoEdge AI™** | Embedded TinyML sub-product & C99 SDK | ARM Cortex-M33 (`CMSIS-DSP` accelerated) | RISC-V `RV32IMFC` (`FMA` accelerated) | Time moments + 4-band Goertzel DSP, Multi-Regime AD (`pe_ad_*`), Few-Shot Classifier (`pe_clf_*`) |

---

### 2. Standardized Codebase & Wire Protocol Conventions

* **Core TinyML Header & Prefix:** `picoedge_ai.h` | Functions: `pe_extract_features()`, `pe_ad_learn()`, `pe_ad_detect()`, `pe_clf_learn_class()`, `pe_clf_predict()`
* **System & Telemetry Prefix:** `harmoniq_hal.h`, `harmoniq_zenoh_wire.h` | Functions: `hmq_*`
* **Wire Frame Structure (16 Bytes Total = 1 DECT NR+ Sub-Slot):**
  * **Zenoh-Pico L2 Header (4 Bytes):** `[Z_DATA (1B)] [Numerical_Key_ID (1B)] [Priority/Flags (1B)] [Payload_Len = 12 (1B)]`
  * **HarmoniQ Payload (`hmq_zenoh_pkt_t`, 12 Bytes):** `[Magic: 0x4851 "HQ" (2B)] [Node_ID (1B)] [Flags (1B)] [Similarity % (1B)] [Active_Regime (1B)] [Predicted_Class (1B)] [Confidence % (1B)] [Most_Deviant_Feat (1B)] [Reserved (1B)] [Seq_Num (2B)]`

---

### 3. The 24-Week (5-Phase) V1 Production Plan

#### Phase 1: PicoEdge AI Core Hardening & HarmoniQ Studio (Weeks 1–4)
**Goal:** Finalize the zero-dependency `picoedge_ai.h` library and `harmoniq_studio.py` AutoML generator with dual ARM/RISC-V CI testing.

1. **MISRA-C & Memory Hardening (`picoedge_ai.h`):**
   * Enforce zero dynamic heap allocation (`malloc`/`free` prohibited), NaN/Inf math guards, and CRC-16 validation on saved model structs.
   * Implement the 10-feature-per-axis extractor (Mean, RMS, StdDev, Kurtosis, Crest Factor, ZCR + 4-Band Goertzel Spectral Energy), Multi-Regime Dynamic Clustering (`PE_MAX_REGIMES = 4`), and On-Device Few-Shot Classifier (`PE_MAX_CLASSES = 6`).
2. **Cross-ISA DSP Abstraction (`harmoniq_hal.h`):**
   * Route vector math to **ARM CMSIS-DSP** (`arm_rms_f32`, `arm_dot_prod_f32`) when compiled for the nRF91x1 (`__ARM_ARCH_8M_MAIN__`), while maintaining an unrolled **RISC-V `RV32IMFC`** path (`__riscv`) verified in QEMU so the codebase remains 100% ready for the LM10xx V2 chip.
3. **HarmoniQ Studio AutoML Upgrades (`harmoniq_studio.py`):**
   * Add automated **Feature Pruning** (generating a compile-time `PE_ACTIVE_FEATS_MASK` if certain Goertzel bands or moments aren't needed for a specific machine) and export signed binary weight packages (`.pe_pkg`) alongside `picoedge_model.h`.

---

#### Phase 2: Native Layer-2 DECT NR+ & Zenoh-Pico Link Layer (Weeks 5–8)
**Goal:** Strip out the IPv6/UDP stack completely and run `zenoh-pico` directly over Nordic’s DECT NR+ MAC/PHY v2.0+ in a 1-hop Star topology.

1. **Zero-IP Zephyr Configuration (`prj.conf`):**
   * Disable `CONFIG_NETWORKING`, `CONFIG_NET_IPV6`, and `CONFIG_NET_UDP` on the **HarmoniQ Sense** build, saving ~50 KB Flash, ~20 KB RAM, and eliminating all ICMPv6 neighbor discovery air traffic.
2. **Custom `zenoh-pico` Native DECT NR+ Link Plugin (`harmoniq_zp_dect_link.c`):**
   * Implement `_z_send_dect_nrplus()` and `_z_read_dect_nrplus()` directly atop Nordic’s **DECT NR+ PHY/MAC (`nrf_modem_dect_phy` / DLC SAP)** using a lockless Zephyr `k_msgq`.
   * Map Zenoh peer endpoints (`dectnr/<RD_ID>`) directly to the native **32-bit DECT NR+ Long Radio Device ID (RD ID)** for unicast (with hardware MAC HARQ acknowledgments) and `0xFFFFFFFF` for zone-wide broadcasts.
3. **1-Byte Numerical Key Registration (`z_declare_keyexpr`):**
   * Bind string topics (`dectroniq/harmoniq/zone1/sense_XX/alert` and `.../state`) to 1-byte VarInt resource IDs during association so no ASCII strings ever cross the airwaves during real-time operation, locking in the **16-byte single-sub-slot (~208 µs)** frame budget.

---

#### Phase 3: Autonomous Sensor DMA, NVS Persistence & Power Budgeting (Weeks 9–13)
**Goal:** Achieve multi-year battery life on **HarmoniQ Sense** nodes using hardware-paced DMA acquisition, Wake-on-Motion, and wear-leveled Flash storage.

1. **CPU-Free Sensor Window Acquisition (nRF91x1 DPPI + EasyDMA):**
   * Interface a low-power industrial MEMS accelerometer (e.g., ST IIS3DWB / LSM6DSV16X or ADI ADXL355) via SPI with hardware FIFO watermark interrupts.
   * Use **nRF91x1 DPPI + SPIM EasyDMA** to burst-transfer the 128×3 window into SRAM while the Cortex-M33 sleeps at ~2 µA, waking the CPU for only **400 µs** to run `pe_ad_detect()`.
2. **Silence-by-Default Radio Policy:**
   * If `similarity_pct >= 75%` and `active_regime == last_regime`, the DECT NR+ radio remains completely powered down. The radio only transmits a 208 µs sub-slot burst on an anomaly, a regime shift, or a scheduled heartbeat.
3. **Wear-Leveled Non-Volatile Knowledge Storage (`storage_nvs.c`):**
   * Keep learned Multi-Regime centroids and Few-Shot class prototypes in Retention RAM across sleep cycles, committing `pe_save_knowledge()` to **Zephyr NVS Flash** only when a new operating regime spawns, when field learning completes, or once per 24 hours.

---

#### Phase 4: HarmoniQ Hub Swarm Correlation, Reflexes & LTE-M Backhaul (Weeks 14–18)
**Goal:** Build the real-time multi-sensor correlation engine, swarm centroid sync, and cellular cloud bridge on the **HarmoniQ Hub (nRF9161)**.

1. **Sub-10ms Multi-Sense Spatial Correlator (`harmoniq_hub_zenoh.c`):**
   * Maintain a real-time **Fleet State Table** in Hub RAM fed by a Zenoh wildcard subscriber (`dectroniq/harmoniq/zone1/*/*`):
     * **Common-Mode Shock Filter:** If $\ge 3$ Sense nodes report a spike within a **20 ms sliding window** on `Kurtosis/RMS`, classify as an ambient floor shock (e.g., passing forklift) and suppress false trips.
     * **Kinematic Mismatch Detector:** Pair drive-end (`sense_A`) and driven-end (`sense_B`) nodes on the same asset; trigger an immediate alert if `sense_A` enters a running regime while `sense_B` remains in `Regime 0 (Idle)`.
     * **Local Reflex Publisher:** On a verified fault, publish a 4-byte emergency command (`Z_PRIORITY_REAL_TIME`) to `dectroniq/harmoniq/zone1/reflex/actuate` in **< 5 ms** to trip a local PLC/relay.
2. **Swarm Centroid Sync (`dectroniq/harmoniq/zone1/broadcast/seed`):**
   * When a technician teaches *one* Sense node a new normal regime or fault class via button press or command, the Hub captures its 120-byte PicoEdge AI centroid and broadcasts it to `dectnr/0xFFFFFFFF`, seeding all identical Sense nodes in the zone simultaneously.
3. **Dual-Modem Scheduling & TrustZone Security:**
   * Use the nRF9161’s hardware time-sliced coexistence to listen continuously on **DECT NR+** while batching verified incident reports and OTA `.pe_pkg` weight updates over **LTE-M** to a cloud Zenoh router.
   * Secure all OTA weight signatures and NVS keys inside **ARM TrustZone (TF-M)** using the nRF9161 CryptoCell-310 engine.

---

#### Phase 5: HIL Validation, UKCA/CE Certification & LM10xx V2 Prep (Weeks 19–24)
**Goal:** Stress-test the multi-node swarm, complete UK/EU regulatory certification, and package the portable SDK for the V2 RISC-V sensor node.

1. **Multi-Node Hardware-in-the-Loop (HIL) Rig:**
   * Connect 4 **HarmoniQ Sense (nRF91x1)** nodes and 1 **HarmoniQ Hub (nRF9161)** to a synchronized signal-injection rig.
   * Replay recorded bearing faults, unbalance harmonics, and simultaneous common-mode floor shocks for 72 hours, verifying **< 5 ms** reflex latency, zero false trips on common-mode shocks, and zero memory leaks.
2. **UKCA, CE & FCC Regulatory Approvals:**
   * Leverage Nordic’s pre-certified nRF9161/nRF9131 SiP modular approvals to fast-track **UKCA** and **CE** compliance (**ETSI EN 301 406-2** for 1.9 GHz DECT NR+ and EN 301 908 for LTE-M), requiring only EMC and radiated spurious emissions testing on the final enclosure.
3. **Factory BIST & LM10xx V2 Pilot Handoff:**
   * Implement a 2-second **Built-In Self-Test (BIST)** on boot that excites the MEMS accelerometer’s internal electrostatic self-test pin and verifies `pe_extract_features()` against factory bounds.
   * Deliver the proven, RISC-V-verified `picoedge_ai.h` + `harmoniq_zp_dect_link.c` package to **Last Mile Semiconductor** as a lead-customer application for bring-up on the **LM10xx** V2 Sense hardware.

---

### Immediate Next Milestone (Week 1 Kick-Off)
With two **Nordic nRF9161 DKs** on the bench (one flashed as **HarmoniQ Sense #1**, one flashed as **HarmoniQ Hub**), you can bring up the entire **PicoEdge AI $\rightarrow$ 16-Byte Zenoh L2 $\rightarrow$ Native DECT NR+ Sub-Slot $\rightarrow$ Hub Wildcard Correlator** loop before designing a single custom PCB.