#!/usr/bin/env python3
"""Compare ready placement samples with one real cached native terrain field.

This measures representation bytes and precision, not runtime or geographic sampling.
The native integration must independently prove identical placement and rendered pixels.
"""
import argparse
import ctypes
import ctypes.util
import json
from pathlib import Path
import sqlite3
import struct


def measure(cache, zstd_library, side):
    if side < 2 or side > 512:
        raise ValueError('sampling side must lie within the native patch budget')
    with sqlite3.connect(cache.resolve().as_uri() + '?mode=ro', uri=True) as db:
        row = db.execute(
            "SELECT p.bytes,p.codec,p.native_bytes,a.key FROM assets a JOIN packages p "
            "ON a.package=p.key WHERE a.kind='terrain-field' AND a.offset=0 "
            "ORDER BY a.id DESC LIMIT 1").fetchone()
    if not row:
        raise ValueError('no prepared terrain field in this cache')
    stored, codec, count, key = row
    if count > 64 * 1024**2:
        raise ValueError('native field exceeds the terrain byte budget')
    if codec == 0:
        data = stored
    elif codec == 1:
        library = zstd_library or ctypes.util.find_library('zstd')
        if not library:
            raise ValueError('libzstd not found; pass --zstd-library')
        lib = ctypes.CDLL(library)
        lib.ZSTD_decompress.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
                                       ctypes.c_void_p, ctypes.c_size_t]
        lib.ZSTD_decompress.restype = ctypes.c_size_t
        lib.ZSTD_isError.argtypes = [ctypes.c_size_t]
        decoded = ctypes.create_string_buffer(count)
        used = lib.ZSTD_decompress(decoded, count, stored, len(stored))
        if lib.ZSTD_isError(used) or used != count:
            raise ValueError('invalid native terrain package')
        data = decoded.raw
    else:
        raise ValueError('unsupported terrain package codec')
    magic, zoom, x, y, rows, cols = struct.unpack_from('<IiIIII', data)
    if magic != 0x31465450 or rows < 2 or cols < 2:
        raise ValueError('unsupported native terrain field')
    samples = memoryview(data)[-rows * cols * 4:]
    selected = []
    for j in range(side):
        row = j * (rows - 1) / (side - 1)
        low = int(row)
        high = min(low + 1, rows - 1)
        for i in range(side):
            col = round(i * (cols - 1) / (side - 1))
            a = struct.unpack_from('<f', samples, (low * cols + col) * 4)[0]
            b = struct.unpack_from('<f', samples, (high * cols + col) * 4)[0]
            selected.append(a + (b - a) * (row - low))
    exact = struct.pack('<' + 'd' * len(selected), *selected)
    rounded = struct.unpack('<' + 'f' * len(selected),
                            struct.pack('<' + 'f' * len(selected), *selected))
    return dict(asset=key, tile=[zoom, x, y], field_shape=[rows, cols],
                field_native_bytes=len(data), field_stored_bytes=len(stored),
                patch_side=side, patch_f64_bytes=24 + len(exact),
                field_to_patch_bytes=len(data) / (24 + len(exact)),
                exact_f64_roundtrip=struct.unpack('<' + 'd' * len(selected), exact) ==
                tuple(selected), f32_changed_samples=sum(a != b for a, b in
                                                        zip(selected, rounded)),
                f32_max_height_error_m=max(abs(a - b) for a, b in zip(selected, rounded)),
                native_runtime_proven=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cache', type=Path)
    parser.add_argument('--zstd-library')
    parser.add_argument('--side', type=int, default=257)
    args = parser.parse_args()
    print(json.dumps(measure(args.cache, args.zstd_library, args.side), indent=2))
