#ifndef PICOEDGE_HAL_H
#define PICOEDGE_HAL_H

#include <stdint.h>

#if defined(CONFIG_CMSIS_DSP) && defined(__ARM_ARCH_8M_MAIN__)
#include <arm_math.h>
#endif

static inline float pe_hal_dot_prod_f32(const float *a, const float *b, uint32_t len) {
#if defined(CONFIG_CMSIS_DSP) && defined(__ARM_ARCH_8M_MAIN__)
    float res;
    arm_dot_prod_f32(a, b, len, &res);
    return res;
#elif defined(__riscv)
    float sum0 = 0.0f, sum1 = 0.0f, sum2 = 0.0f, sum3 = 0.0f;
    uint32_t i = 0;
    for (; i + 3 < len; i += 4) {
        sum0 += a[i] * b[i];
        sum1 += a[i+1] * b[i+1];
        sum2 += a[i+2] * b[i+2];
        sum3 += a[i+3] * b[i+3];
    }
    for (; i < len; i++) {
        sum0 += a[i] * b[i];
    }
    return sum0 + sum1 + sum2 + sum3;
#else
    float sum = 0.0f;
    for (uint32_t i = 0; i < len; i++) {
        sum += a[i] * b[i];
    }
    return sum;
#endif
}

#endif // PICOEDGE_HAL_H
