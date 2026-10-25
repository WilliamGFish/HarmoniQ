# HarmoniQ PicoEdge AI™ — Improvement & Extension Plan

While the V1 implementation of the PicoEdge AI suite successfully realizes the core structural, algorithmic, and footprint requirements laid out in the WBS, there are several key areas for improvement and extension to make the platform fully production-ready and scaleable.

## 1. Algorithmic & Transpilation Enhancements

*   **True Model Transpilation to C99:**
    Currently, the AutoML pipeline (`harmoniq_studio.py`) identifies the optimal ML architecture via 5-Fold Cross Validation and generates the `.pe_pkg` binary payload for hotloading. However, the static `#include "picoedge_model.h"` fallback generation uses a mock.
    *   **Action Item:** Implement AST-based C99 generation natively in Python for `DecisionTreeClassifier`, traversing the `tree_` attribute to generate nested `if/else` logic. (Note: True C99 transpilation for `LinearSVC` is already implemented and working).
*   **Dynamic Goertzel Frequencies:**
    The feature extractor currently hardcodes 4 Goertzel bins at normalized frequencies $f/f_s \in \{0.05, 0.12, 0.25, 0.40\}$.
    *   **Action Item:** Allow these coefficients to be computed at runtime during `pe_init` based on a user-provided array of frequencies and the actual sensor sampling rate, allowing targeting of specific bearing defect frequencies (BPFO, BPFI, BSF).
*   **Advanced Feature Sets:**
    *   **Action Item:** Extend `picoedge_dsp.c` to include configurable MFCCs (Mel-Frequency Cepstral Coefficients) for acoustic monitoring applications, and Spectral Kurtosis for advanced impulse detection.

## 2. Platform & Embedded RTOS Integration

*   **DECT NR+ and Zenoh Transport Layer:**
    The library supports hotloading `.pe_pkg` weight blobs and exporting/importing `.pe_knowledge` blobs. This is designed to integrate natively with DECT NR+ (ETSI 103636 v2.2.1) using Zenoh as the transport protocol.
    *   **Action Item:** Build a companion module that hooks into the DECT NR+ firmware library using Zenoh publishers and subscribers to expose PicoEdge telemetry (Anomaly Streak, Active Regime, Predictions) and supports receiving OTA weight updates natively over the DECT NR+ mesh.
*   **Dynamic Axis and Sensor Fusion:**
    `CONFIG_PE_AXES` is currently a compile-time static configuration.
    *   **Action Item:** Evolve the `pe_context` to handle multiple independent sensor streams (e.g., a 3-axis accelerometer and a 1-axis microphone) with separate regimes and feature masks for each stream.

## 3. Tooling & Studio Improvements

*   **Python Packaging (PyPI):**
    *   **Action Item:** Refactor the `studio/` directory into a formal Python package (`harmoniq-studio`) with a `pyproject.toml`, allowing users to install it via `pip install harmoniq-studio` and use it as a CLI application.
*   **Hardware-in-the-Loop (HIL) Testing:**
    *   **Action Item:** Expand the `nrf9161dk_bench` sample into a fully automated HIL test script using `pytest-twister` to run the test suite directly on physical Cortex-M33 silicon, verifying DWT cycle counting in real-time.

## 4. Maintenance & MISRA Compliance

*   **Static Code Analysis:**
    *   **Action Item:** Further harden the `cppcheck` CI pipeline to include strict MISRA-C 2012 compliance checking.
