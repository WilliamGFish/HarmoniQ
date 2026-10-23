Building a production-ready TinyML product across both the **Nordic nRF91x1 series (nRF9161/nRF9131)** and the **Last Mile Semiconductor LM10xx series** is a strong architectural pairing. 

Both SoCs are built around the **DECT NR+ (1.9 GHz non-cellular 5G)** standard, which provides license-free, low-latency, high-reliability mesh and star networking. In a commercial product ecosystem, they naturally complement each other:
* **Last Mile Semi LM10xx (Edge Sensor Node):** A 22nm FDSOI, 200 MHz **RISC-V** SoC with an integrated DECT NR+ transceiver, Edge AI acceleration, and a quantum-proof secure element. It is designed for ultra-low-cost, battery- or energy-harvested sensor nodes deployed in dense DECT NR+ meshes.
* **Nordic nRF91x1 (Heavy Edge Node or NR+-to-Cellular Gateway):** A 64 MHz **ARM Cortex-M33** SiP supporting **both DECT NR+ and LTE-M / NB-IoT** (plus GNSS). It can run the exact same TinyML engine locally *or* act as the field gateway that aggregates DECT NR+ mesh alerts from dozens of LM10xx nodes and backhauls them to the cloud over cellular LTE-M.

Because you are targeting two different CPU instruction sets (**ARMv8-M Cortex-M33** vs. **32-bit RISC-V**), your production plan must enforce a strict separation between the **portable ANSI C99 TinyML core** and the **Hardware Abstraction Layer (HAL)**.

This **5-Phase Production Plan (24 Weeks)** outlines the transition from the open-source TinyML engine to a certified, commercial product on both SoCs.

---

### Target Hardware Comparison & Allocation

| Feature | Nordic nRF91x1 (nRF9161 / nRF9131) | Last Mile Semi LM10xx Series | Impact on TinyML Architecture |
| :--- | :--- | :--- | :--- |
| **CPU Core & ISA** | 64 MHz **ARM Cortex-M33** (FPU + DSP) | Up to 200 MHz **RISC-V** (22nm FDSOI) | Requires cross-ISA DSP macros (CMSIS-DSP vs. RISC-V RV32IMFC / NMSIS) |
| **Connectivity** | **DECT NR+** + **LTE-M / NB-IoT** + GNSS | **DECT NR+** (1.9 GHz, up to 10 Mbps, 6 km) | Unified IPv6 / CoAP / CBOR payload format across both chips |
| **Security** | Arm TrustZone (TF-M) + CryptoCell-310 | Integrated Quantum-Proof Secure Element | Signed OTA weight updates & encrypted NVS model storage |
| **Primary Role** | Standalone Cellular Sensor *or* NR+ Gateway | High-density, ultra-low-power NR+ Mesh Sensor | Identical C99 TinyML engine runs on both; gateway adds fleet aggregation |

---

### Phase 1: Core Engine Hardening & Cross-ISA Abstraction (Weeks 1–4)
**Objective:** Turn the prototype C header into a deterministic, MISRA-C compliant library that compiles with hardware-accelerated math on both ARM Cortex-M33 and RISC-V.

1. **MISRA-C & Memory Safety Hardening:**
   * Guarantee **zero dynamic heap allocation** (`malloc`/`free` strictly forbidden). All model structs (`ae_anomaly_engine_t`, `ae_ondevice_classifier_t`) and working buffers are statically allocated at compile time.
   * Add NaN/Inf guards, underflow clamping, and CRC-16 checksums to the model state structs so corrupted RAM or Flash never causes a hard fault.
2. **Cross-ISA Math & DSP HAL (`ae_dsp_hal.h`):**
   * Abstract vector dot products, RMS, variance, and Goertzel loops behind inline macros:
     * **On nRF91x1 (`__ARM_ARCH_8M_MAIN__`):** Route math to **ARM CMSIS-DSP** (`arm_rms_f32`, `arm_dot_prod_f32`, `arm_var_f32`) utilizing single-cycle SIMD/FPU instructions.
     * **On LM10xx (`__riscv`):** Route math to **RISC-V FPU / P-extension (DSP)** intrinsics (or LMS Edge AI SDK primitives) and provide a **Fixed-Point (`Q15`/`Q31`)** compile-time fallback (`#define AE_USE_FIXED_POINT`) if configuring an ultra-low-power core without FPU active.
3. **Dual-Toolchain CI Pipeline:**
   * Set up a GitHub Actions / GitLab CI runner that compiles the TinyML engine simultaneously on `arm-none-eabi-gcc` (via nRF Connect SDK) and `riscv32-unknown-elf-gcc`, running unit tests in **QEMU / Renode** emulators for both architectures to verify bit-accurate feature parity.

---

### Phase 2: Low-Power Sensor DMA & AutoML Data Pipeline (Weeks 5–8)
**Objective:** Build deterministic, CPU-free sensor acquisition and finalize the offline AutoML Studio toolchain.

1. **CPU-Free Autonomous Window Capture:**
   * High-frequency vibration/acoustic sampling will drain a battery if the CPU wakes up for every sample.
   * **On nRF91x1:** Configure **DPPI (Distributed Programmable Peripheral Interconnect) + SPIM/TWIM EasyDMA** paired with an external sensor FIFO (e.g., LSM6DSV16X or ADXL355). The CPU stays in deep sleep (`System ON Idle` @ ~2 µA) while the sensor collects 128 samples, waking the Cortex-M33 only once per window via a FIFO watermark interrupt.
   * **On LM10xx:** Configure the RISC-V peripheral DMA controller and low-power timer trigger to burst-read the sensor FIFO into SRAM before waking the main RISC-V compute core.
2. **Field Data Logger Firmware:**
   * Build a "Data Collection Mode" firmware flag that streams raw sensor windows + metadata (device ID, temperature, supply voltage) over **DECT NR+ UDP sockets** (at up to 10 Mbps) or high-speed UART to a local laptop/gateway.
3. **AutoML Studio Production Upgrade (`argon_studio.py`):**
   * Add **Automated Feature Pruning (Recursive Feature Elimination):** If only 8 out of the 30 extracted features are needed to achieve 99% accuracy, disable the unused Goertzel bands or time moments at compile time via bitmasks (`#define AE_ACTIVE_FEATS_MASK`) to cut inference energy by 60%.
   * Export model weights as **versioned binary blobs (`.ae_pkg`)** in addition to `.h` headers, enabling Over-the-Air (OTA) weight replacement without reflashing the entire firmware.

---

### Phase 3: RTOS Architecture, Flash Persistence & Power Budgeting (Weeks 9–13)
**Objective:** Integrate the TinyML engine into Zephyr RTOS / FreeRTOS with crash-safe non-volatile storage and aggressive power management.

1. **Multi-Threaded RTOS Isolation (Zephyr RTOS):**
   * Use **Zephyr RTOS** (natively supported by Nordic's nRF Connect SDK and standard across modern RISC-V IoT SDKs) to isolate real-time tasks:
     * `Thread 1 (High Priority - Sensor DMA):` Handles FIFO watermark interrupts and pushes raw windows into a lockless `k_msgq`.
     * `Thread 2 (Med Priority - TinyML Engine):` Pulls windows, runs `ae_ad_detect()` and `ae_clf_predict()` (< 0.5 ms execution), and logs state transitions.
     * `Thread 3 (Low Priority - DECT NR+ / LTE-M Comms):` Handles network handshakes, retries, and encrypted payload transmission.
2. **Wear-Leveled Non-Volatile Knowledge Storage (NVS):**
   * When learning in the field (`ae_ad_learn` or `ae_clf_learn_class`), saving directly to raw Flash after every window will wear out Flash sectors.
   * Implement a dirty-flag commit policy using **Zephyr NVS (Non-Volatile Storage)** or **LittleFS**: keep learned centroids in Retention RAM during deep sleep, and commit `ae_save_knowledge()` to Flash only when a new operating regime spawns, when learning finishes, or once every 24 hours.
3. **Two-Stage "Wake-on-Event" Power Architecture:**
   * **Stage 0:** Sensor hardware Wake-on-Motion (WOM) interrupt triggers when vibration exceeds a physical threshold.
   * **Stage 1:** MCU wakes for 0.4 ms, runs local TinyML inference. If `similarity > 85%` (normal), the radio **never turns on**—the MCU goes straight back to sleep.
   * **Stage 2:** Radio wakes *only* to transmit a compact anomaly alert or a scheduled daily health heartbeat.

---

### Phase 4: DECT NR+ Telemetry, Security & OTA Model Updates (Weeks 14–18)
**Objective:** Connect the edge intelligence to the network securely and enable remote model updates.

1. **Compact CBOR Telemetry over CoAP / LwM2M:**
   * Instead of verbose JSON, serialize TinyML outputs into a **24-byte CBOR packet** over CoAP/IPv6 (natively supported over both DECT NR+ and LTE-M):
     * `Device_ID` (4B) | `Timestamp` (4B) | `Similarity_Score` (uint8, 0–100) | `Active_Regime` (uint8) | `Predicted_Class` (uint8) | `Confidence` (uint8) | `Top_3_Anomalous_Features` (12B).
2. **Hot-Swapping TinyML Models Over-the-Air (OTA):**
   * Full firmware OTA updates over low-power wireless are slow and power-hungry.
   * Because the classifier weights and anomaly centroids are only **~1.5 KB**, create a dedicated **LwM2M Object (e.g., Object ID 33500 "TinyML Model")**. This allows you to push updated AutoML weights or reset/seed baseline centroids across thousands of LM10xx and nRF91x1 nodes in seconds.
3. **Hardware Security Integration:**
   * **On nRF91x1:** Store the NVS encryption keys and verify signed OTA model packages inside **ARM TrustZone (TF-M)** using the CryptoCell-310 accelerator.
   * **On LM10xx:** Route model signature verification and DECT NR+ link keys through the chip’s **Quantum-Proof Secure Element**, ensuring industrial attackers cannot inject spoofed fault models or tamper with learned baselines.

---

### Phase 5: HIL Validation, RF Certification & Factory Provisioning (Weeks 19–24)
**Objective:** Validate reliability under harsh industrial conditions and prepare for mass production.

1. **Hardware-in-the-Loop (HIL) Signal Injection:**
   * Build an automated test rig using a DAC/SPI emulator that injects pre-recorded industrial fault waveforms (plus thermal drift and supply voltage sag) directly into the nRF91x1 and LM10xx boards.
   * Verify zero memory leaks, watchdog timer (WDT) recovery, and >95% classification agreement between the Python AutoML studio and both embedded targets over a 72-hour stress test.
2. **Regulatory & RF Certification:**
   * **DECT NR+ (Both Chips):** ETSI EN 301 406-2 compliance testing for the 1.9 GHz band.
   * **Cellular (nRF91x1):** Leverage Nordic’s pre-certified SiP modular approvals for GCF/PTCRB and carrier certifications, requiring only radiated spurious emissions (RSE) testing for your custom PCB antenna.
3. **Factory Built-In Self-Test (BIST) & Auto-Calibration:**
   * On the assembly line, flash a factory test mode that excites the sensor's internal self-test electrostatic actuator, runs `ae_extract_features()`, and verifies that noise floor and sensitivity fall within factory tolerance before sealing the enclosure.

---

### Reference Implementation: Cross-SoC Glue Layer (`ae_platform_glue.h`)
This structural template demonstrates how the firmware seamlessly compiles across both the **nRF91x1 (ARM Cortex-M33)** and **LM10xx (RISC-V)** while handling hardware DSP, NVS Flash persistence, and compact telemetry packing:

```c
#ifndef AE_PLATFORM_GLUE_H
#define AE_PLATFORM_GLUE_H

#include "argon_edge_ai.h"
#include <stdint.h>
#include <stdbool.h>

// 1. CROSS-ISA HARDWARE DSP ACCELERATION
#if defined(__ARM_ARCH_8M_MAIN__) // Nordic nRF91x1 (ARM Cortex-M33)
    #include <arm_math.h>
    static inline float ae_hw_dot_prod(const float *a, const float *b, uint32_t len) {
        float result;
        arm_dot_prod_f32(a, b, len, &result);
        return result;
    }
#elif defined(__riscv)            // Last Mile Semi LM10xx (RISC-V)
    static inline float ae_hw_dot_prod(const float *a, const float *b, uint32_t len) {
        float sum = 0.0f;
        // Unrolled for RISC-V RV32IMFC single-cycle FMA (fmadd.s) pipeline
        uint32_t i = 0;
        for (; i + 3 < len; i += 4) {
            sum += a[i]*b[i] + a[i+1]*b[i+1] + a[i+2]*b[i+2] + a[i+3]*b[i+3];
        }
        for (; i < len; i++) sum += a[i] * b[i];
        return sum;
    }
#endif

// 2. COMPACT 12-BYTE DECT NR+ / LTE-M TELEMETRY PACKET
typedef struct __attribute__((packed)) {
    uint16_t magic_header;      // 0xAE01
    uint8_t  status_flags;      // Bit 0: Anomaly Alert, Bit 1: Learning Done
    uint8_t  similarity_pct;    // 0 - 100%
    uint8_t  active_regime;     // 0 - (AE_MAX_REGIMES - 1)
    uint8_t  predicted_class;   // 0 - (AE_MAX_CLASSES - 1)
    uint8_t  class_confidence;  // 0 - 100%
    uint8_t  most_deviant_feat; // Index of feature that triggered the anomaly
    uint32_t sample_counter;    // Total windows processed
} ae_nrplus_packet_t;

// Packs inference results and identifies the exact physical feature causing the anomaly
static void ae_pack_telemetry(const ae_anomaly_engine_t *eng, float similarity,
                              uint8_t regime, uint8_t pred_class, float conf,
                              const float *current_feats, ae_nrplus_packet_t *pkt) {
    pkt->magic_header     = 0xAE01;
    pkt->status_flags     = (similarity < 75.0f ? 0x01 : 0x00) | 
                            (eng->total_samples >= eng->min_samples ? 0x02 : 0x00);
    pkt->similarity_pct   = (uint8_t)(similarity + 0.5f);
    pkt->active_regime    = regime;
    pkt->predicted_class  = pred_class;
    pkt->class_confidence = (uint8_t)(conf * 100.0f + 0.5f);
    pkt->sample_counter   = eng->total_samples;

    // Root-cause diagnostics: Find which feature deviated most from the matched regime
    float max_z = -1.0f;
    uint8_t worst_f = 0;
    uint32_t denom = (eng->total_samples > 1) ? (eng->total_samples - 1) : 1;
    for (uint8_t i = 0; i < AE_NUM_FEATURES; i++) {
        float var = eng->global_m2[i] / (float)denom;
        float diff = current_feats[i] - eng->centroids[regime][i];
        float z = (diff * diff) / (var + AE_EPSILON);
        if (z > max_z) { max_z = z; worst_f = i; }
    }
    pkt->most_deviant_feat = worst_f; // Tells cloud if fault is RMS, Kurtosis, or Spectral Band!
}

#endif // AE_PLATFORM_GLUE_H
```

### Recommended Next Step to Kick Off Phase 1
Since the **nRF9161 DK** is readily available with full Zephyr + DECT NR+ + LTE-M support in the nRF Connect SDK, I recommend starting **Phase 1 & 2** on the nRF9161 DK while keeping the RISC-V cross-compilation check active for the **LM10xx** target. 

Would you like me to generate the **Zephyr RTOS `prj.conf` and multi-threaded `main.c` template** (including Zephyr NVS Flash storage and DECT NR+ socket transmission) to jumpstart your Phase 1 & 3 repository?