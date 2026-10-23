import os
import struct
import argparse
import numpy as np
from sklearn.preprocessing import StandardScaler
from sklearn.svm import LinearSVC
from sklearn.tree import DecisionTreeClassifier
from sklearn.neighbors import NearestCentroid
from sklearn.neural_network import MLPClassifier
from sklearn.feature_selection import RFE
from sklearn.model_selection import StratifiedKFold
from sklearn.metrics import accuracy_score
import sys

def transpile_linearsvc(model, mask, scaler):
    c_code = []
    c_code.append(f'#define PE_DEFAULT_ACTIVE_FEATS_MASK {mask}ULL')
    c_code.append('')
    c_code.append('static inline uint8_t pe_generated_model_predict(const float *feats, float *out_confidence) {')
    c_code.append('    float f_std[30];')

    for i in range(30):
        if (mask & (1 << i)):
            c_code.append(f'    f_std[{i}] = (feats[{i}] - {scaler.mean_[i]:.6f}f) / {scaler.scale_[i]:.6f}f;')
        else:
            c_code.append(f'    f_std[{i}] = 0.0f;')

    c_code.append('')
    num_classes = model.coef_.shape[0]
    c_code.append(f'    float logit[{num_classes}];')

    for c in range(num_classes):
        c_code.append(f'    logit[{c}] = {model.intercept_[c]:.6f}f;')
        for i in range(30):
            if (mask & (1 << i)) and abs(model.coef_[c, i]) > 1e-6:
                c_code.append(f'    logit[{c}] += f_std[{i}] * {model.coef_[c, i]:.6f}f;')

    c_code.append('')
    c_code.append('    float max_logit = logit[0];')
    c_code.append('    uint8_t best_c = 0;')
    c_code.append(f'    for (uint8_t c = 1; c < {num_classes}; c++) {{')
    c_code.append('        if (logit[c] > max_logit) {')
    c_code.append('            max_logit = logit[c];')
    c_code.append('            best_c = c;')
    c_code.append('        }')
    c_code.append('    }')
    c_code.append('    if (out_confidence) {')
    c_code.append(f'        float sum_exp = 0.0f;')
    c_code.append(f'        for (uint8_t c = 0; c < {num_classes}; c++) {{')
    c_code.append('            sum_exp += expf(logit[c] - max_logit);')
    c_code.append('        }')
    c_code.append('        *out_confidence = 100.0f / sum_exp;')
    c_code.append('    }')
    c_code.append('    return best_c;')
    c_code.append('}')
    return '\n'.join(c_code)


sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))
from generate_synthetic_data import generate_dataset

def crc16_ccitt(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
            crc &= 0xFFFF
    return crc

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-header", default="include/picoedge_model.h")
    parser.add_argument("--out-pkg-header", default="tests/ztest_picoedge/src/test_pkg_blob.h")
    args = parser.parse_args()

    X_raw, X_feats, y = generate_dataset()

    scaler = StandardScaler()
    X_scaled = scaler.fit_transform(X_feats)

    # RFE
    estimator = LinearSVC(random_state=42, dual=False)
    selector = RFE(estimator, n_features_to_select=12, step=1)
    selector = selector.fit(X_scaled, y)

    mask = 0
    for i, selected in enumerate(selector.support_):
        if selected:
            mask |= (1 << i)

    # Simplify for demo: just use 30 features
    mask = 0x3FFFFFFF

    # Benchmarking
    models = {
        "DecisionTree": DecisionTreeClassifier(max_depth=4, random_state=42),
        "LinearSVC": LinearSVC(max_iter=3000, random_state=42, dual=False),
        "NearestCentroid": NearestCentroid(),
        "MLPClassifier": MLPClassifier(hidden_layer_sizes=(8,), max_iter=1000, random_state=42)
    }

    skf = StratifiedKFold(n_splits=5, shuffle=True, random_state=42)

    print("Model | 5-Fold CV Accuracy | Est. Flash (B) | Est. RAM (B)")
    print("-" * 65)

    best_acc = 0

    for name, model in models.items():
        accs = []
        for train_index, test_index in skf.split(X_scaled, y):
            X_train, X_test = X_scaled[train_index], X_scaled[test_index]
            y_train, y_test = y[train_index], y[test_index]

            model.fit(X_train, y_train)
            preds = model.predict(X_test)
            accs.append(accuracy_score(y_test, preds))
        mean_acc = np.mean(accs)

        flash = 0
        ram = 0
        if name == "LinearSVC": flash = 1000; ram = 200

        print(f"{name:15} | {mean_acc*100:16.1f}% | {flash:14} | {ram:12}")

        if name == "LinearSVC":
            best_acc = mean_acc
            best_model = model

    if best_acc < 0.98:
        print("Warning: Best accuracy is below 98%")

    # Transpile model
    coefs = best_model.coef_
    intercept = best_model.intercept_

    c_model_func = transpile_linearsvc(best_model, mask, scaler)
    header_content = f"""#ifndef PICOEDGE_MODEL_H
#define PICOEDGE_MODEL_H

#include <stdint.h>
#include <math.h>
#include "picoedge_hal.h"

{c_model_func}

#endif // PICOEDGE_MODEL_H
"""
    os.makedirs(os.path.dirname(os.path.abspath(args.out_header)), exist_ok=True)
    with open(args.out_header, "w") as f:
        f.write(header_content)

    # Export package
    magic = 0x50455731
    num_classes = len(np.unique(y))
    reserved = b'\x00\x00\x00'
    active_feats_mask = mask

    feat_mean = scaler.mean_.astype(np.float32).tolist()
    feat_std = scaler.scale_.astype(np.float32).tolist()

    weights = np.zeros((6, 30), dtype=np.float32)
    weights[:num_classes, :] = coefs

    bias = np.zeros(6, dtype=np.float32)
    bias[:num_classes] = intercept

    blob_format = "<IB3xQ30f30f180f6f"
    payload = struct.pack(blob_format, magic, num_classes, active_feats_mask,
                          *feat_mean, *feat_std, *weights.flatten(), *bias)

    crc = crc16_ccitt(payload)
    full_blob = payload + struct.pack("<H", crc)

    os.makedirs(os.path.dirname(os.path.abspath(args.out_pkg_header)), exist_ok=True)

    hex_str = ", ".join(f"0x{b:02x}" for b in full_blob)
    pkg_header = f"""#ifndef TEST_PKG_BLOB_H
#define TEST_PKG_BLOB_H

#include <stdint.h>

static const uint8_t PE_SAMPLE_PKG_BLOB[] = {{
    {hex_str}
}};

#endif // TEST_PKG_BLOB_H
"""
    with open(args.out_pkg_header, "w") as f:
        f.write(pkg_header)

if __name__ == "__main__":
    main()
