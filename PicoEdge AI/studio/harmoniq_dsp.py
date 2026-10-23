import numpy as np

def extract_features_py(raw_window: np.ndarray, active_feats_mask: int = 0xFFFFFFFFFFFFFFFF) -> np.ndarray:
    raw_window = np.asarray(raw_window, dtype=np.float32)
    window_size, axes = raw_window.shape
    out_feats = np.zeros(axes * 10, dtype=np.float32)

    GOERTZEL_COEFFS = np.array([1.902113, 1.457937, 0.000000, -1.618034], dtype=np.float32)
    PE_EPSILON = np.float32(1e-5)

    for ch in range(axes):
        base = ch * 10
        if ((active_feats_mask >> base) & 0x3FF) == 0:
            continue

        chan_data = raw_window[:, ch]

        sum_val = np.float32(0.0)
        sq_sum_val = np.float32(0.0)
        min_v = chan_data[0]
        max_v = chan_data[0]

        for val in chan_data:
            sum_val += val
            sq_sum_val += val * val
            if val < min_v: min_v = val
            if val > max_v: max_v = val

        mean = sum_val / np.float32(window_size)
        rms = np.sqrt(sq_sum_val / np.float32(window_size), dtype=np.float32)

        var_sum = np.float32(0.0)
        fourth_moment = np.float32(0.0)
        zcr_count = 0
        prev_diff = chan_data[0] - mean
        s1 = np.zeros(4, dtype=np.float32)
        s2 = np.zeros(4, dtype=np.float32)
        spec_mask = (active_feats_mask >> (base + 6)) & 0x0F

        for i in range(window_size):
            diff = chan_data[i] - mean
            d2 = diff * diff
            var_sum += d2
            fourth_moment += d2 * d2

            if i > 0 and ((diff >= 0.0 and prev_diff < 0.0) or (diff < 0.0 and prev_diff >= 0.0)):
                zcr_count += 1
            prev_diff = diff

            if spec_mask != 0:
                for b in range(4):
                    s0 = np.float32(diff + GOERTZEL_COEFFS[b] * s1[b] - s2[b])
                    s2[b] = s1[b]
                    s1[b] = s0

        variance = var_sum / np.float32(window_size)
        std_dev = np.sqrt(variance if variance > 0.0 else np.float32(0.0), dtype=np.float32)
        kurtosis = (fourth_moment / np.float32(window_size)) / (variance * variance + PE_EPSILON)
        max_abs = np.abs(max_v) if np.abs(max_v) > np.abs(min_v) else np.abs(min_v)
        crest = max_abs / (rms + PE_EPSILON)
        zcr = np.float32(zcr_count) / np.float32(window_size)

        out_feats[base + 0] = mean
        out_feats[base + 1] = rms
        out_feats[base + 2] = std_dev
        out_feats[base + 3] = kurtosis
        out_feats[base + 4] = crest
        out_feats[base + 5] = zcr

        for b in range(4):
            power = s1[b]*s1[b] + s2[b]*s2[b] - GOERTZEL_COEFFS[b]*s1[b]*s2[b]
            out_feats[base + 6 + b] = np.log10((np.abs(power) / np.float32(window_size)) + np.float32(1.0), dtype=np.float32)

        for k in range(base, base + 10):
            if ((active_feats_mask >> k) & 1) == 0:
                out_feats[k] = np.float32(0.0)

    return out_feats
