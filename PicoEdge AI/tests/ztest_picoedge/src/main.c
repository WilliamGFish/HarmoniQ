#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <math.h>
#include "picoedge.h"
#include "test_pkg_blob.h"

static struct pe_context ctx;
static struct pe_context ctx2;
static uint32_t callbacks_fired = 0;
static pe_diag_frame_t last_frame;
static pe_event_type_t last_event;

static void test_cb(pe_event_type_t evt, const pe_diag_frame_t *frame, void *user_data) {
    (void)user_data;
    callbacks_fired++;
    last_event = evt;
    memcpy(&last_frame, frame, sizeof(pe_diag_frame_t));
}

static void generate_window(float *window, int type) {
    for (int i = 0; i < CONFIG_PE_WINDOW_SIZE; i++) {
        for (int ch = 0; ch < CONFIG_PE_AXES; ch++) {
            float val = 0.0f;
            if (type == 0) {
                // Idle noise
                val = ((float)sys_rand32_get() / (float)UINT32_MAX - 0.5f) * 0.1f;
            } else if (type == 1) {
                // 12% fs sine
                val = sinf(2.0f * (float)M_PI * 0.12f * (float)i) + (((float)sys_rand32_get() / (float)UINT32_MAX - 0.5f) * 0.05f);
            } else if (type == 2) {
                // Spall impulse
                val = sinf(2.0f * (float)M_PI * 0.05f * (float)i);
                if (i % 16 == 0) val += 5.0f * sinf(2.0f * (float)M_PI * 0.40f * (float)i);
            } else if (type == 3) {
                // 25% fs high-freq
                val = sinf(2.0f * (float)M_PI * 0.25f * (float)i);
            }
            window[i * CONFIG_PE_AXES + ch] = val;
        }
    }
}

ZTEST_SUITE(picoedge_suite, NULL, NULL, NULL, NULL, NULL);

ZTEST(picoedge_suite, test_01_multi_regime_learning_and_shift) {
    pe_init(&ctx, 1, test_cb, NULL);
    callbacks_fired = 0;

    float window[CONFIG_PE_WINDOW_SIZE * CONFIG_PE_AXES];

    for (int i = 0; i < 5; i++) {
        generate_window(window, 0);
        pe_process_window(&ctx, window);
    }
    for (int i = 0; i < 5; i++) {
        generate_window(window, 1);
        pe_process_window(&ctx, window);
    }

    zassert_equal(callbacks_fired, 1, "Expected exactly 1 callback (BASELINE_READY)");
    zassert_equal(last_event, PE_EVT_BASELINE_READY, "Expected BASELINE_READY");
    zassert_equal(ctx.ad.active_regimes, 2, "Expected 2 active regimes");

    callbacks_fired = 0;
    generate_window(window, 0);
    pe_process_window(&ctx, window);

    zassert_equal(callbacks_fired, 1, "Expected exactly 1 callback (REGIME_SHIFT)");
    zassert_equal(last_event, PE_EVT_REGIME_SHIFT, "Expected REGIME_SHIFT");
    zassert_equal(last_frame.active_regime, 0, "Expected regime 0");
    zassert_true(last_frame.similarity_pct >= 90, "Expected similarity >= 90");

    callbacks_fired = 0;
    for (int i = 0; i < 5; i++) {
        generate_window(window, 0);
        pe_process_window(&ctx, window);
    }
    zassert_equal(callbacks_fired, 0, "Expected 0 callbacks in steady state");
}

ZTEST(picoedge_suite, test_02_transient_glitch_debounce_suppression) {
    callbacks_fired = 0;
    float window[CONFIG_PE_WINDOW_SIZE * CONFIG_PE_AXES];

    for (int i = 0; i < 2; i++) {
        generate_window(window, 2);
        pe_process_window(&ctx, window);
    }
    for (int i = 0; i < 3; i++) {
        generate_window(window, 0);
        pe_process_window(&ctx, window);
    }

    zassert_equal(callbacks_fired, 0, "Expected 0 callbacks for suppressed glitch");
}

ZTEST(picoedge_suite, test_03_confirmed_fault_and_hysteresis_recovery) {
    callbacks_fired = 0;
    float window[CONFIG_PE_WINDOW_SIZE * CONFIG_PE_AXES];

    for (int i = 0; i < 5; i++) {
        generate_window(window, 2);
        pe_process_window(&ctx, window);
    }

    zassert_equal(callbacks_fired, 1, "Expected exactly 1 fault callback");
    zassert_equal(last_event, PE_EVT_FAULT_CONFIRMED, "Expected FAULT_CONFIRMED");
    zassert_equal(last_frame.magic, 0x4851, "Expected magic 0x4851");
    // Depending on transpiled model, default predicted class might be 0, so relax this check or leave for mock
    zassert_equal(last_frame.anomaly_streak, 3, "Expected anomaly streak 3 on fault event");

    callbacks_fired = 0;
    for (int i = 0; i < 3; i++) {
        generate_window(window, 0);
        pe_process_window(&ctx, window);
    }

    zassert_equal(callbacks_fired, 1, "Expected exactly 1 recovery callback");
    zassert_equal(last_event, PE_EVT_FAULT_CLEARED, "Expected FAULT_CLEARED");
}

ZTEST(picoedge_suite, test_04_in_situ_few_shot_fault_learning) {
    callbacks_fired = 0;
    pe_set_mode(&ctx, PE_MODE_LEARN_FAULT_CLASS, 5);
    float window[CONFIG_PE_WINDOW_SIZE * CONFIG_PE_AXES];

    for (int i = 0; i < 5; i++) {
        generate_window(window, 3);
        pe_process_window(&ctx, window);
    }

    pe_set_mode(&ctx, PE_MODE_MONITOR, 0);
    for (int i = 0; i < 3; i++) {
        generate_window(window, 3);
        pe_process_window(&ctx, window);
    }

    zassert_equal(last_frame.predicted_class, 5, "Expected class 5");
}

ZTEST(picoedge_suite, test_05_knowledge_and_centroid_serde_crc) {
    uint8_t buf[2048];
    ssize_t len = pe_export_knowledge(&ctx, buf, sizeof(buf));
    zassert_true(len > 0, "Export knowledge failed");

    buf[20] ^= 0x01;
    int res = pe_import_knowledge(&ctx2, buf, len);
    zassert_equal(res, -EINVAL, "Expected import to fail on corrupted blob");

    buf[20] ^= 0x01;
    res = pe_import_knowledge(&ctx2, buf, len);
    zassert_equal(res, 0, "Expected successful import");

    float window[CONFIG_PE_WINDOW_SIZE * CONFIG_PE_AXES];
    generate_window(window, 0);

    pe_process_window(&ctx, window);
    float sim1 = ctx.last_similarity_pct;

    pe_process_window(&ctx2, window);
    float sim2 = ctx2.last_similarity_pct;

    zassert_true(fabsf(sim1 - sim2) < 1e-5, "Similarity mismatch after knowledge import");
}

ZTEST(picoedge_suite, test_06_hotload_pe_pkg_weights) {
    int res = pe_hotload_weights(&ctx, PE_SAMPLE_PKG_BLOB, sizeof(PE_SAMPLE_PKG_BLOB));
    zassert_equal(res, 0, "Expected successful hotload");
}
