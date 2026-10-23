# PicoEdge AI™ — User Guide & How-To Instructions

Welcome to the PicoEdge AI Embedded TinyML Library. This guide will walk you through integrating the library into your Zephyr RTOS project, using the C API for on-device anomaly detection, and training custom fault classifiers using HarmoniQ Studio.

## 1. Integrating with Zephyr RTOS

PicoEdge AI is built as a Zephyr External Module. To add it to your Zephyr workspace:

1.  **Add to `west.yml`:**
    In your application's `west.yml` manifest, add the repository:
    ```yaml
    manifest:
      projects:
        - name: picoedge-ai
          path: modules/lib/picoedge-ai
          revision: main
    ```
    Then run `west update`.

2.  **Enable Kconfig Symbols:**
    In your application's `prj.conf`, enable the library and configure the required window size (which must match your sensor DMA buffers):
    ```ini
    CONFIG_PICOEDGE_AI=y
    CONFIG_PE_WINDOW_SIZE=128
    CONFIG_PE_AXES=3
    ```

## 2. On-Device C API Usage

The library operates on raw floating-point sensor windows. It handles state transitions (Learning $\rightarrow$ Nominal $\rightarrow$ Suspect $\rightarrow$ Fault) internally.

### Initialization

Include the header and initialize the context with an event callback.

```c
#include <picoedge.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

static struct pe_context pe_ctx;

// The callback receives events like BASELINE_READY, REGIME_SHIFT, and FAULT_CONFIRMED.
static void picoedge_event_handler(pe_event_type_t evt, const pe_diag_frame_t *frame, void *user_data) {
    switch(evt) {
        case PE_EVT_BASELINE_READY:
            LOG_INF("PicoEdge: Baseline established. Monitoring active.");
            break;
        case PE_EVT_FAULT_CONFIRMED:
            LOG_ERR("PicoEdge: FAULT CONFIRMED! Predicted Class: %u", frame->predicted_class);
            break;
        case PE_EVT_FAULT_CLEARED:
            LOG_INF("PicoEdge: Fault condition cleared.");
            break;
        case PE_EVT_REGIME_SHIFT:
            LOG_INF("PicoEdge: Operating regime shift detected.");
            break;
    }
}

void main(void) {
    // Initialize context for Node ID 1
    pe_init(&pe_ctx, 1, picoedge_event_handler, NULL);

    // Begin feeding sensor data
    start_sensor_dma();
}
```

### Processing Sensor Data

Whenever your sensor completes a DMA transfer (e.g., 128 samples of X, Y, Z interleaved), pass it to `pe_process_window`:

```c
void on_dma_complete(const float *raw_buffer) {
    // Process the 128x3 window (execution time typically < 400us on Cortex-M33)
    int err = pe_process_window(&pe_ctx, raw_buffer);
    if (err) {
        LOG_ERR("Processing failed: %d", err);
    }
}
```

### Few-Shot On-Device Fault Learning

To teach the system a specific fault "in situ" (without writing Python code):

1.  Put the machine in the faulty state.
2.  Switch to learn mode for a specific Class ID (e.g., 2):
    `pe_set_mode(&pe_ctx, PE_MODE_LEARN_FAULT_CLASS, 2);`
3.  Let the sensor run for 5-10 windows to learn the RBF prototype.
4.  Return to monitoring mode:
    `pe_set_mode(&pe_ctx, PE_MODE_MONITOR, 0);`

## 3. Training Custom Models with HarmoniQ Studio

For advanced classification, you can collect raw data, train a model on your PC, and push the weights back to the edge device.

### Installation

Install the Python dependencies:
```bash
cd picoedge-ai
pip install -r studio/requirements.txt
```

### Generating Models and Payloads

1.  **Format Your Data:** Ensure your dataset is shaped `(N_windows, 128, 3)` for `X_raw` and integer labels for `y`.
2.  **Run HarmoniQ Studio:** The studio script extracts features identically to the C library, runs Recursive Feature Elimination (RFE), benchmarks architectures, and generates outputs.
    ```bash
    python3 studio/harmoniq_studio.py --out-header include/picoedge_model.h --out-pkg-header my_app/src/test_pkg_blob.h
    ```
3.  **Output 1 (`include/picoedge_model.h`):** The transpiled static C model.
4.  **Output 2 (`test_pkg_blob.h`):** A binary `.pe_pkg` payload that can be sent over MQTT/LwM2M.

### Hotloading OTA Weights

To apply a `.pe_pkg` payload at runtime over-the-air without rebooting:

```c
// Suppose 'payload_buf' contains the received bytes and 'payload_len' is the size
int err = pe_hotload_weights(&pe_ctx, payload_buf, payload_len);
if (err == 0) {
    LOG_INF("Successfully hotloaded new Linear SVM weights from cloud!");
}
```
