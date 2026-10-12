#!/usr/bin/env python3
"""Compare cached capture data with the existing flat-card render payload.

Reads one real native package without modifying the cache. Counts RGBA8 maps and
quad attributes; codec metadata, compression and native frame time remain unmeasured.
"""
import argparse
import ctypes
import ctypes.util
import hashlib
import json
from pathlib import Path
import sqlite3
import struct
import zlib

from prepared_building_residency import unpack


def dimensions(data):
    if len(data) < 72:
        raise ValueError('truncated atlas')
    magic, version, _, pixels, views, surfaces = struct.unpack_from('<QIQIII', data)
    if magic != 0x004e574f5243534f or version not in (1, 2):
        raise ValueError('unsupported atlas')
    if not 3 <= pixels <= 4096 or not 1 <= views <= 64 or not 1 <= surfaces <= 64:
        raise ValueError('invalid atlas dimensions')
    count = pixels * pixels * views
    if count > 1 << 24:
        raise ValueError('atlas texel budget exceeded')
    stride = 20 if version == 1 else 40
    expected = 64 + surfaces * 52 + views * 24 + count * stride + 8
    if len(data) != expected:
        raise ValueError('atlas length disagrees with dimensions')
    return pixels, views, surfaces, count, stride


def measure(cache, library=None):
    library = library or ctypes.util.find_library('zstd')
    if not library:
        raise ValueError('libzstd is required')
    lib = ctypes.CDLL(library)
    lib.ZSTD_decompress.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
                                   ctypes.c_void_p, ctypes.c_size_t]
    lib.ZSTD_decompress.restype = ctypes.c_size_t
    lib.ZSTD_isError.argtypes = [ctypes.c_size_t]
    with sqlite3.connect(cache.resolve().as_uri() + '?mode=ro', uri=True) as db:
        record = db.execute("SELECT a.key,a.package,p.bytes,p.crc,p.codec,p.native_bytes "
                            "FROM assets a JOIN packages p ON a.package=p.key "
                            "WHERE a.kind='impostor-prototype' ORDER BY a.id DESC LIMIT 1").fetchone()
    if record is None:
        raise ValueError('no cached prototype')
    key, package, stored, crc, codec, native = record
    if zlib.crc32(stored) != crc:
        raise ValueError('stored package checksum failed')
    data = unpack(lib, stored, codec, native)
    if hashlib.sha256(data).hexdigest() != package:
        raise ValueError('native package digest failed')
    pixels, views, surfaces, count, stride = dimensions(data)
    for broken in (data[:-1], data + b'\0'):
        try:
            dimensions(broken)
        except ValueError:
            pass
        else:
            raise ValueError('length validation accepted corrupt input')
    maps = count * 3 * 4
    quad = views * (4 * (3 + 3 + 4 + 2) * 4 + 6 * 4)
    return {'asset': key, 'package': package, 'pixels': pixels, 'views': views,
            'surfaces': surfaces, 'capture_bytes_per_texel': stride,
            'stored_bytes': len(stored), 'capture_native_bytes': len(data),
            'ready_rgba8_bytes': maps, 'ready_quad_attribute_bytes': quad,
            'ready_payload_lower_bound_bytes': maps + quad,
            'scope': 'existing flat render cards; payload size only, no residency or timing proof'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cache', type=Path)
    parser.add_argument('--zstd-library')
    args = parser.parse_args()
    print(json.dumps(measure(args.cache, args.zstd_library), indent=2))
