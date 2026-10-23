#include "picoedge_core.h"
#include <math.h>
#include <string.h>

static const float GOERTZEL_COEFFS[4] = { 1.902113f, 1.457937f, 0.000000f, -1.618034f };

void pe_extract_features_masked(const float *raw_window, float *out_feats, uint64_t active_feats_mask) {
    for (int ch = 0; ch < CONFIG_PE_AXES; ch++) {
        int base = ch * PE_FEATS_PER_AXIS;
        if (((active_feats_mask >> base) & 0x3FFU) == 0) {
            memset(&out_feats[base], 0, 10 * sizeof(float));
            continue;
        }

        float sum = 0.0f, sq_sum = 0.0f, min_v = raw_window[ch], max_v = raw_window[ch];
        for (int i = 0; i < CONFIG_PE_WINDOW_SIZE; i++) {
            float val = raw_window[i * CONFIG_PE_AXES + ch];
            sum += val;
            sq_sum += val * val;
            if (val < min_v) min_v = val;
            if (val > max_v) max_v = val;
        }

        float mean = sum / (float)CONFIG_PE_WINDOW_SIZE;
        float rms = sqrtf(sq_sum / (float)CONFIG_PE_WINDOW_SIZE);

        float var_sum = 0.0f, fourth_moment = 0.0f;
        int zcr_count = 0;
        float prev_diff = raw_window[ch] - mean;
        float s1[4] = {0}, s2[4] = {0};
        uint32_t spec_mask = (active_feats_mask >> (base + 6)) & 0x0FU;

        for (int i = 0; i < CONFIG_PE_WINDOW_SIZE; i++) {
            float diff = raw_window[i * CONFIG_PE_AXES + ch] - mean;
            float d2 = diff * diff;
            var_sum += d2;
            fourth_moment += d2 * d2;

            if (i > 0 && ((diff >= 0.0f && prev_diff < 0.0f) || (diff < 0.0f && prev_diff >= 0.0f))) {
                zcr_count++;
            }
            prev_diff = diff;

            if (spec_mask != 0) {
                for (int b = 0; b < 4; b++) {
                    float s0 = diff + GOERTZEL_COEFFS[b] * s1[b] - s2[b];
                    s2[b] = s1[b];
                    s1[b] = s0;
                }
            }
        }

        float variance = var_sum / (float)CONFIG_PE_WINDOW_SIZE;
        float std_dev = sqrtf(variance > 0.0f ? variance : 0.0f);
        float kurtosis = (fourth_moment / (float)CONFIG_PE_WINDOW_SIZE) / (variance * variance + PE_EPSILON);
        float max_abs = (fabsf(max_v) > fabsf(min_v)) ? fabsf(max_v) : fabsf(min_v);
        float crest = max_abs / (rms + PE_EPSILON);
        float zcr = (float)zcr_count / (float)CONFIG_PE_WINDOW_SIZE;

        out_feats[base + 0] = mean;
        out_feats[base + 1] = rms;
        out_feats[base + 2] = std_dev;
        out_feats[base + 3] = kurtosis;
        out_feats[base + 4] = crest;
        out_feats[base + 5] = zcr;

        for (int b = 0; b < 4; b++) {
            float power = s1[b]*s1[b] + s2[b]*s2[b] - GOERTZEL_COEFFS[b]*s1[b]*s2[b];
            out_feats[base + 6 + b] = log10f((fabsf(power) / (float)CONFIG_PE_WINDOW_SIZE) + 1.0f);
        }

        for (int k = base; k < base + 10; k++) {
            if (((active_feats_mask >> k) & 1U) == 0) {
                out_feats[k] = 0.0f;
            }
        }
    }
}
