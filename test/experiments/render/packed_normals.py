"""Compare Float3 upload with the existing Oct16x2 normal representation."""

import argparse
import ctypes
import ctypes.util
import hashlib
import json
from pathlib import Path
import sqlite3
import struct

import numpy as np


def fold(unit):
    v = unit / np.sum(np.abs(unit), axis=1, keepdims=True)
    xy = v[:, :2]
    return np.where(v[:, 2:3] < 0, (1 - np.abs(xy[:, ::-1])) * np.where(xy >= 0, 1, -1), xy)


def unpack(words):
    xy = np.c_[words >> 16, words & 65535].astype(np.float32) / np.float32(65535) * 2 - 1
    z = 1 - np.sum(np.abs(xy), axis=1, keepdims=True)
    xy = np.where(z < 0, (1 - np.abs(xy[:, ::-1])) * np.where(xy >= 0, 1, -1), xy)
    v = np.c_[xy, z].astype(np.float64)
    return v / np.linalg.norm(v, axis=1, keepdims=True)


def cached_words(path):
    with sqlite3.connect(path.resolve().as_uri() + '?mode=ro', uri=True) as db:
        key, package, start, count = db.execute(
            "SELECT key,package,offset,bytes FROM assets WHERE kind='building-lod' ORDER BY bytes DESC LIMIT 1").fetchone()
        blob, codec, size = db.execute('SELECT bytes,codec,native_bytes FROM packages WHERE key=?', (package,)).fetchone()
    if codec:
        library = ctypes.CDLL(ctypes.util.find_library('zstd'))
        library.ZSTD_decompress.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t]
        library.ZSTD_decompress.restype = ctypes.c_size_t
        data = ctypes.create_string_buffer(size)
        assert library.ZSTD_decompress(data, size, blob, len(blob)) == size
        blob = data.raw
    data = memoryview(blob)[start:start + count]
    magic = b'outshine-structure'
    assert data[:len(magic)] == magic and struct.unpack_from('<I', data, len(magic))[0] == 9
    assert hashlib.sha256(data[:-64]).hexdigest().encode() == data[-64:]
    at = len(magic) + 4 + 64
    arrays = []
    dtype = np.dtype([('values', '<f4', 5), ('normal', '<u4')])
    for _ in range(2):
        n = struct.unpack_from('<Q', data, at)[0]
        at += 8
        arrays.append(np.frombuffer(data, dtype=dtype, count=n, offset=at)['normal'])
        at += n * dtype.itemsize
    return key, np.concatenate(arrays)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', type=Path)
    args = parser.parse_args()
    unit = np.random.default_rng(47).normal(size=(500000, 3))
    unit /= np.linalg.norm(unit, axis=1, keepdims=True)
    pair = np.floor((fold(unit) * .5 + .5) * 65535 + .5).astype(np.uint32)
    words = (pair[:, 0] << 16) | pair[:, 1]
    decoded = unpack(words)
    error = np.arctan2(np.linalg.norm(np.cross(unit, decoded), axis=1), np.sum(unit * decoded, axis=1))
    bound = np.arcsin(np.sqrt(18) / 65535)
    assert error.max() < bound + 1e-6
    report = dict(samples=len(unit), angle_bound_rad=float(bound), max_angle_rad=float(error.max()))
    if args.assets:
        key, stored = cached_words(args.assets)
        report.update(asset=key, stored_normals=len(stored), float3_bytes=len(stored) * 12, oct_bytes=len(stored) * 4)
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
