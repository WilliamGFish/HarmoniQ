#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "picoedge.h"

static struct pe_context ctx;

static void bench_cb(pe_event_type_t evt, const pe_diag_frame_t *frame, void *user_data) {
    (void)user_data;
    printk("[CB] Event: %d, Sim: %u%%, Regime: %u, Class: %u\n",
           evt, frame->similarity_pct, frame->active_regime, frame->predicted_class);
}

int main(void) {
    printk("Starting PicoEdge Bench...\n");
    pe_init(&ctx, 1, bench_cb, NULL);

    float window[CONFIG_PE_WINDOW_SIZE * CONFIG_PE_AXES] = {0};

    while (1) {
        uint32_t start = k_cycle_get_32();
        pe_process_window(&ctx, window);
        uint32_t end = k_cycle_get_32();

        uint32_t us = k_cyc_to_us_floor32(end - start);

        printk("[PicoEdge AI] Window processed in %u us | Sim: %u%% | Regime: %u | Class: %u\n",
               us, (uint32_t)ctx.last_similarity_pct, ctx.active_regime, ctx.predicted_class);

        k_sleep(K_MSEC(100));
    }

    return 0;
}
