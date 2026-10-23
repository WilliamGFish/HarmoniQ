import os
import ctypes
import pytest
import numpy as np
import subprocess
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from harmoniq_dsp import extract_features_py

LIB_PATH = "/tmp/libpicoedge_parity.so"

@pytest.fixture(scope="session", autouse=True)
def compile_c_lib():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
    cmd = [
        "gcc", "-shared", "-fPIC", "-O2", "-std=c99",
        f"-I{os.path.join(repo_root, 'include')}",
        os.path.join(repo_root, 'src', 'picoedge_dsp.c'),
        os.path.join(repo_root, 'src', 'picoedge_core.c'),
        "-lm", "-o", LIB_PATH
    ]
    subprocess.check_call(cmd)

@pytest.fixture
def c_lib():
    lib = ctypes.CDLL(LIB_PATH)
    lib.pe_extract_features_masked.argtypes = [
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_float),
        ctypes.c_uint32
    ]
    return lib

def test_dsp_bit_parity_random_and_sine(c_lib):
    np.random.seed(42)
    window_size = 128
    axes = 3

    for _ in range(50):
        # Generate some random/sine data
        t = np.linspace(0, 1, window_size, endpoint=False)
        raw_window = np.zeros((window_size, axes), dtype=np.float32)
        raw_window[:, 0] = np.random.randn(window_size)
        raw_window[:, 1] = np.sin(2 * np.pi * 5 * t) + np.random.randn(window_size) * 0.1
        raw_window[:, 2] = np.random.randn(window_size) * 2 + 1.0 # DC offset

        mask = 0x3FFFFFFF

        py_feats = extract_features_py(raw_window, mask)

        c_feats = np.zeros(30, dtype=np.float32)
        raw_flat = raw_window.flatten().astype(np.float32)

        c_lib.pe_extract_features_masked(
            raw_flat.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
            c_feats.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
            mask
        )

        np.testing.assert_allclose(py_feats, c_feats, atol=1e-4, rtol=1e-4)

def test_feature_masking_parity(c_lib):
    np.random.seed(42)
    window_size = 128
    axes = 3

    raw_window = np.random.randn(window_size, axes).astype(np.float32)
    masks = [0x15555555, 0x000003FF, 0x00000000]

    for mask in masks:
        py_feats = extract_features_py(raw_window, mask)

        c_feats = np.zeros(30, dtype=np.float32)
        raw_flat = raw_window.flatten().astype(np.float32)

        c_lib.pe_extract_features_masked(
            raw_flat.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
            c_feats.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
            mask
        )

        np.testing.assert_allclose(py_feats, c_feats, atol=1e-4, rtol=1e-4)
