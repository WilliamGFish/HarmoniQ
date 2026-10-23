import numpy as np
import os

def generate_dataset(samples_per_class=120, seed=42):
    np.random.seed(seed)
    window_size = 128
    axes = 3
    num_classes = 4

    total_samples = samples_per_class * num_classes
    X_raw = np.zeros((total_samples, window_size, axes), dtype=np.float32)
    y_labels = np.zeros(total_samples, dtype=np.int32)

    n = np.arange(window_size)

    idx = 0
    # Class 0: Idle
    for _ in range(samples_per_class):
        X_raw[idx] = np.random.normal(0, 0.04, (window_size, axes)).astype(np.float32)
        y_labels[idx] = 0
        idx += 1

    # Class 1: Healthy Run
    for i in range(samples_per_class):
        f = 0.05 if i < samples_per_class // 2 else 0.12
        signal = (1.0 + np.random.normal(0, 0.08, window_size)) * np.sin(2 * np.pi * f * n)
        X_raw[idx] = np.tile(signal[:, np.newaxis], (1, axes)).astype(np.float32)
        y_labels[idx] = 1
        idx += 1

    # Class 2: Shaft Unbalance
    for _ in range(samples_per_class):
        signal = 2.5 * np.sin(2 * np.pi * 0.05 * n) + 1.2 * np.sin(2 * np.pi * 0.12 * n) + np.random.normal(0, 0.15, window_size)
        X_raw[idx] = np.tile(signal[:, np.newaxis], (1, axes)).astype(np.float32)
        y_labels[idx] = 2
        idx += 1

    # Class 3: Bearing Spall Impacts
    for _ in range(samples_per_class):
        signal = np.sin(2 * np.pi * 0.05 * n)
        impacts = np.zeros(window_size)
        impacts[::16] = 5.0 * np.sin(2 * np.pi * 0.40 * n[::16])
        signal += impacts + np.random.normal(0, 0.05, window_size)
        X_raw[idx] = np.tile(signal[:, np.newaxis], (1, axes)).astype(np.float32)
        y_labels[idx] = 3
        idx += 1

    from harmoniq_dsp import extract_features_py
    X_feats = np.zeros((total_samples, 30), dtype=np.float32)
    for i in range(total_samples):
        X_feats[i] = extract_features_py(X_raw[i])

    return X_raw, X_feats, y_labels

if __name__ == "__main__":
    X_raw, X_feats, y = generate_dataset()
    print(f"X_raw shape: {X_raw.shape}")
    print(f"X_feats shape: {X_feats.shape}")

    assert X_raw.shape == (480, 128, 3)
    assert X_feats.shape == (480, 30)
    print("Dataset generation verified.")
