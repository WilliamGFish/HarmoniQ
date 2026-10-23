#ifndef PICOEDGE_MODEL_H
#define PICOEDGE_MODEL_H

#include <stdint.h>
#include <math.h>
#include "picoedge_hal.h"

#define PE_DEFAULT_ACTIVE_FEATS_MASK 1073741823ULL

static inline uint8_t pe_generated_model_predict(const float *feats, float *out_confidence) {
    float f_std[30];
    f_std[0] = (feats[0] - 0.044400f) / 0.047672f;
    f_std[1] = (feats[1] - 0.959187f) / 0.699634f;
    f_std[2] = (feats[2] - 0.957924f) / 0.698330f;
    f_std[3] = (feats[3] - 4.299963f) / 5.065208f;
    f_std[4] = (feats[4] - 2.802559f) / 1.272147f;
    f_std[5] = (feats[5] - 0.230046f) / 0.160811f;
    f_std[6] = (feats[6] - 1.149316f) / 0.939266f;
    f_std[7] = (feats[7] - 0.620603f) / 0.774158f;
    f_std[8] = (feats[8] - 0.016636f) / 0.020137f;
    f_std[9] = (feats[9] - 0.148544f) / 0.245989f;
    f_std[10] = (feats[10] - 0.044531f) / 0.047558f;
    f_std[11] = (feats[11] - 0.959074f) / 0.699783f;
    f_std[12] = (feats[12] - 0.957801f) / 0.698491f;
    f_std[13] = (feats[13] - 4.290357f) / 5.071921f;
    f_std[14] = (feats[14] - 2.796504f) / 1.268788f;
    f_std[15] = (feats[15] - 0.230225f) / 0.161222f;
    f_std[16] = (feats[16] - 1.149301f) / 0.939284f;
    f_std[17] = (feats[17] - 0.620594f) / 0.774166f;
    f_std[18] = (feats[18] - 0.016651f) / 0.020127f;
    f_std[19] = (feats[19] - 0.148562f) / 0.245978f;
    f_std[20] = (feats[20] - 0.044588f) / 0.047497f;
    f_std[21] = (feats[21] - 0.959090f) / 0.699762f;
    f_std[22] = (feats[22] - 0.957827f) / 0.698458f;
    f_std[23] = (feats[23] - 4.296663f) / 5.067402f;
    f_std[24] = (feats[24] - 2.806891f) / 1.273178f;
    f_std[25] = (feats[25] - 0.230046f) / 0.161356f;
    f_std[26] = (feats[26] - 1.149264f) / 0.939330f;
    f_std[27] = (feats[27] - 0.620597f) / 0.774163f;
    f_std[28] = (feats[28] - 0.016635f) / 0.020138f;
    f_std[29] = (feats[29] - 0.148549f) / 0.245986f;

    float logit[4];
    logit[0] = -0.487862f;
    logit[0] += f_std[0] * 0.051930f;
    logit[0] += f_std[1] * -0.007452f;
    logit[0] += f_std[2] * -0.007664f;
    logit[0] += f_std[3] * -0.095215f;
    logit[0] += f_std[4] * 0.128985f;
    logit[0] += f_std[5] * 0.301135f;
    logit[0] += f_std[6] * 0.039514f;
    logit[0] += f_std[7] * -0.087750f;
    logit[0] += f_std[8] * 0.009465f;
    logit[0] += f_std[9] * -0.062112f;
    logit[0] += f_std[10] * 0.047034f;
    logit[0] += f_std[11] * -0.007771f;
    logit[0] += f_std[12] * -0.007998f;
    logit[0] += f_std[13] * -0.099399f;
    logit[0] += f_std[14] * 0.092436f;
    logit[0] += f_std[15] * 0.281982f;
    logit[0] += f_std[16] * 0.039595f;
    logit[0] += f_std[17] * -0.087522f;
    logit[0] += f_std[18] * 0.009430f;
    logit[0] += f_std[19] * -0.062172f;
    logit[0] += f_std[20] * 0.052363f;
    logit[0] += f_std[21] * -0.007931f;
    logit[0] += f_std[22] * -0.008138f;
    logit[0] += f_std[23] * -0.097623f;
    logit[0] += f_std[24] * 0.124551f;
    logit[0] += f_std[25] * 0.308808f;
    logit[0] += f_std[26] * 0.039434f;
    logit[0] += f_std[27] * -0.087799f;
    logit[0] += f_std[28] * 0.009558f;
    logit[0] += f_std[29] * -0.062190f;
    logit[1] = -0.654121f;
    logit[1] += f_std[0] * -0.102647f;
    logit[1] += f_std[1] * -0.129673f;
    logit[1] += f_std[2] * -0.129888f;
    logit[1] += f_std[3] * 0.062211f;
    logit[1] += f_std[4] * -0.387100f;
    logit[1] += f_std[5] * -0.442183f;
    logit[1] += f_std[6] * -0.157810f;
    logit[1] += f_std[7] * 0.049451f;
    logit[1] += f_std[8] * -0.002724f;
    logit[1] += f_std[9] * 0.030515f;
    logit[1] += f_std[10] * -0.095363f;
    logit[1] += f_std[11] * -0.128337f;
    logit[1] += f_std[12] * -0.128439f;
    logit[1] += f_std[13] * 0.076820f;
    logit[1] += f_std[14] * -0.204798f;
    logit[1] += f_std[15] * -0.428599f;
    logit[1] += f_std[16] * -0.157483f;
    logit[1] += f_std[17] * 0.048788f;
    logit[1] += f_std[18] * -0.010399f;
    logit[1] += f_std[19] * 0.031075f;
    logit[1] += f_std[20] * -0.095798f;
    logit[1] += f_std[21] * -0.126780f;
    logit[1] += f_std[22] * -0.126926f;
    logit[1] += f_std[23] * 0.083328f;
    logit[1] += f_std[24] * -0.229646f;
    logit[1] += f_std[25] * -0.492116f;
    logit[1] += f_std[26] * -0.157194f;
    logit[1] += f_std[27] * 0.049197f;
    logit[1] += f_std[28] * -0.005295f;
    logit[1] += f_std[29] * 0.030752f;
    logit[2] = -0.476177f;
    logit[2] += f_std[0] * 0.109831f;
    logit[2] += f_std[1] * 0.101325f;
    logit[2] += f_std[2] * 0.101228f;
    logit[2] += f_std[3] * -0.073066f;
    logit[2] += f_std[4] * 0.061802f;
    logit[2] += f_std[5] * 0.150879f;
    logit[2] += f_std[6] * 0.103652f;
    logit[2] += f_std[7] * 0.050891f;
    logit[2] += f_std[8] * 0.029130f;
    logit[2] += f_std[9] * -0.061067f;
    logit[2] += f_std[10] * 0.109417f;
    logit[2] += f_std[11] * 0.101253f;
    logit[2] += f_std[12] * 0.101152f;
    logit[2] += f_std[13] * -0.074141f;
    logit[2] += f_std[14] * 0.053458f;
    logit[2] += f_std[15] * 0.139893f;
    logit[2] += f_std[16] * 0.103633f;
    logit[2] += f_std[17] * 0.050887f;
    logit[2] += f_std[18] * 0.030621f;
    logit[2] += f_std[19] * -0.061075f;
    logit[2] += f_std[20] * 0.109833f;
    logit[2] += f_std[21] * 0.101181f;
    logit[2] += f_std[22] * 0.101089f;
    logit[2] += f_std[23] * -0.073775f;
    logit[2] += f_std[24] * 0.052601f;
    logit[2] += f_std[25] * 0.138393f;
    logit[2] += f_std[26] * 0.103618f;
    logit[2] += f_std[27] * 0.050889f;
    logit[2] += f_std[28] * 0.030257f;
    logit[2] += f_std[29] * -0.061091f;
    logit[3] = -0.305214f;
    logit[3] += f_std[0] * -0.047341f;
    logit[3] += f_std[1] * 0.016922f;
    logit[3] += f_std[2] * 0.017134f;
    logit[3] += f_std[3] * 0.168706f;
    logit[3] += f_std[4] * 0.083560f;
    logit[3] += f_std[5] * -0.055102f;
    logit[3] += f_std[6] * -0.016995f;
    logit[3] += f_std[7] * -0.009893f;
    logit[3] += f_std[8] * -0.006392f;
    logit[3] += f_std[9] * 0.151372f;
    logit[3] += f_std[10] * -0.047241f;
    logit[3] += f_std[11] * 0.017672f;
    logit[3] += f_std[12] * 0.017872f;
    logit[3] += f_std[13] * 0.177327f;
    logit[3] += f_std[14] * 0.114469f;
    logit[3] += f_std[15] * -0.047887f;
    logit[3] += f_std[16] * -0.016832f;
    logit[3] += f_std[17] * -0.009880f;
    logit[3] += f_std[18] * -0.000749f;
    logit[3] += f_std[19] * 0.151641f;
    logit[3] += f_std[20] * -0.058074f;
    logit[3] += f_std[21] * 0.016870f;
    logit[3] += f_std[22] * 0.017083f;
    logit[3] += f_std[23] * 0.170432f;
    logit[3] += f_std[24] * 0.097926f;
    logit[3] += f_std[25] * -0.044573f;
    logit[3] += f_std[26] * -0.016824f;
    logit[3] += f_std[27] * -0.009899f;
    logit[3] += f_std[28] * -0.002902f;
    logit[3] += f_std[29] * 0.151238f;

    float max_logit = logit[0];
    uint8_t best_c = 0;
    for (uint8_t c = 1; c < 4; c++) {
        if (logit[c] > max_logit) {
            max_logit = logit[c];
            best_c = c;
        }
    }
    if (out_confidence) {
        float sum_exp = 0.0f;
        for (uint8_t c = 0; c < 4; c++) {
            sum_exp += expf(logit[c] - max_logit);
        }
        *out_confidence = 100.0f / sum_exp;
    }
    return best_c;
}

#endif // PICOEDGE_MODEL_H
