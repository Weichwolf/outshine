"""Measure exact position/index-only building pages on captured asset keys."""

import argparse
import ctypes
import ctypes.util
import json
import sqlite3
import struct
import subprocess
from contextlib import closing
from hashlib import sha256
from pathlib import Path
from time import perf_counter

import numpy as np


class Packages:
    def __init__(self, connection):
        self.db = connection
        path = ctypes.util.find_library("zstd")
        if path is None:
            directory = subprocess.check_output(
                ["pkg-config", "--variable=libdir", "libzstd"], text=True
            ).strip()
            path = next(
                str(p)
                for p in Path(directory).glob("libzstd.*")
                if p.suffix in (".dylib", ".so")
            )
        self.zstd = ctypes.CDLL(path)
        buffer = (ctypes.c_void_p, ctypes.c_size_t)
        for name in ("ZSTD_compressBound", "ZSTD_decompress", "ZSTD_compress"):
            fn = getattr(self.zstd, name)
            fn.restype = ctypes.c_size_t
            fn.argtypes = {
                "ZSTD_compressBound": (ctypes.c_size_t,),
                "ZSTD_decompress": buffer + buffer,
                "ZSTD_compress": buffer + buffer + (ctypes.c_int,),
            }[name]

    def read(self, key):
        sql = """SELECT coalesce(body.bytes,p.bytes), p.codec, p.native_bytes,
                        a.offset, a.bytes
                 FROM assets a JOIN packages p ON a.package=p.key
                 LEFT JOIN package_bodies body ON body.key=p.key
                 WHERE a.key=? AND a.kind='building-static-lod'"""
        record = self.db.execute(sql, (key,)).fetchone()
        if record is None:
            raise ValueError(f"no static building package for {key}")
        stored, codec, native, offset, size = record
        data = stored
        if codec == 1:
            decoded = ctypes.create_string_buffer(native)
            if (
                self.zstd.ZSTD_decompress(decoded, native, stored, len(stored))
                != native
            ):
                raise ValueError("invalid building package")
            data = memoryview(decoded).cast("B")
        elif codec != 0:
            raise ValueError(f"unknown package codec {codec}")
        return memoryview(data)[offset : offset + size], len(stored)

    def encoded_size(self, data):
        capacity = self.zstd.ZSTD_compressBound(len(data))
        result = ctypes.create_string_buffer(capacity)
        stored = self.zstd.ZSTD_compress(result, capacity, data, len(data), 1)
        if stored > capacity:
            raise ValueError("cannot encode shadow package")
        return min(len(data), stored)


def geometry(payload, key):
    magic = b"outshine-structure"
    version, token = struct.unpack_from("<I64s", payload, len(magic))
    if (
        payload[: len(magic)] != magic
        or version not in (13, 14, 15)
        or token != key.encode()
        or sha256(payload[:-64]).hexdigest().encode() != payload[-64:]
    ):
        raise ValueError(f"invalid version, identity or digest for {key}")
    at = len(magic) + 4 + 64

    def take(code, width=1):
        nonlocal at
        length = struct.unpack_from("<Q", payload, at)[0]
        at += 8
        array = np.frombuffer(payload, dtype=code, count=length * width, offset=at)
        at += array.nbytes
        return array.reshape(-1, width) if width > 1 else array

    if version == 15:
        corners, wall_count, roof_count = struct.unpack_from("<QQQ", payload, at)
        at += 24
        lanes = np.frombuffer(payload, dtype="u1", count=corners * 12, offset=at)
        words = lanes.reshape(3, 4, corners).transpose(0, 2, 1).copy().view("<u4")
        positions = np.bitwise_xor.accumulate(words.reshape(3, corners), axis=1).T
        at += lanes.nbytes
        references = np.frombuffer(
            payload,
            dtype="<u2" if corners <= 65536 else "<u4",
            count=wall_count + roof_count,
            offset=at,
        )
        at += references.nbytes + 12 * (wall_count + roof_count)
        walls = positions[references[:wall_count]]
        roofs = positions[references[wall_count:]]
    else:
        walls, roofs = take("<u4", 6)[:, :3], take("<u4", 6)[:, :3]
    wall_direct, roof_direct = take("<u4"), take("<u4")
    cluster_words = 15 if version >= 14 else 13
    take("<u4", cluster_words)
    take("<f4")  # WallColours
    take("<f4")  # WallMaterialM
    wall_clustered = take("<u4")
    take("<u4", cluster_words)
    roof_clustered = take("<u4")
    return (
        walls,
        roofs,
        wall_direct if wall_direct.size else wall_clustered,
        roof_direct if roof_direct.size else roof_clustered,
    )


def shadow_packet(walls, roofs, wall_indices, roof_indices, weld=True):
    if (
        wall_indices.size % 3
        or roof_indices.size % 3
        or np.any(wall_indices >= len(walls))
        or np.any(roof_indices >= len(roofs))
    ):
        raise ValueError("invalid triangle surface")
    # Follow the ordering produced by the respective original or clustered index stream.
    combined = np.concatenate((walls, roofs))
    indices = np.concatenate((wall_indices, roof_indices + len(walls)))
    positions = combined
    selected = indices
    if weld:
        positions, mapping = np.unique(combined, axis=0, return_inverse=True)
        selected = mapping.reshape(-1)[indices]
    packed = selected.astype("<u2" if len(positions) <= 65536 else "<u4")
    if not np.array_equal(positions[packed], combined[indices]):
        raise AssertionError("the shadow triangles, positions or winding changed")
    return (
        struct.pack("<II", len(positions), len(packed))
        + positions.tobytes()
        + packed.tobytes()
    )


def measure(archive, keys):
    size = np.zeros(7, dtype=np.int64)
    welded_seconds = 0.0
    direct_seconds = 0.0
    for key in keys:
        source, stored = archive.read(key)
        vertices = geometry(source, key)
        started = perf_counter()
        direct = shadow_packet(*vertices, weld=False)
        direct_seconds += perf_counter() - started
        started = perf_counter()
        shadow = shadow_packet(*vertices)
        welded_seconds += perf_counter() - started
        size += (
            len(source),
            stored,
            len(direct),
            archive.encoded_size(direct),
            len(shadow),
            archive.encoded_size(shadow),
            (len(vertices[2]) + len(vertices[3])) // 3,
        )
    return dict(
        zip(
            (
                "native_bytes",
                "stored_bytes",
                "direct_native_bytes",
                "direct_stored_bytes",
                "welded_native_bytes",
                "welded_stored_bytes",
                "triangles",
            ),
            size.tolist(),
        )
    ) | {
        "pages": len(keys),
        "direct_ms": round(direct_seconds * 1000, 2),
        "welded_ms": round(welded_seconds * 1000, 2),
    }


def verify():
    wall = np.array([[0, 0, 0], [1, 2, 3], [1, 0, 2]], dtype="<u4")
    roof = np.array([[1, 2, 3], [1, 0, 2], [5, 6, 7]], dtype="<u4")
    assert struct.unpack_from(
        "<II", shadow_packet(wall, roof, np.arange(3), np.arange(3))
    ) == (4, 6)
    wide = np.arange(0, 65537 * 3, dtype="<u4").reshape(-1, 3)
    packet = shadow_packet(
        wide, np.empty((0, 3), "<u4"), np.array([65535, 0, 65536]), np.empty(0, "<u4")
    )
    assert len(packet) == 8 + wide.nbytes + 3 * 4


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("database", type=Path)
    parser.add_argument(
        "keys",
        type=Path,
        nargs="+",
        help="one cached static building asset key per line",
    )
    args = parser.parse_args()
    verify()
    uri = args.database.resolve().as_uri() + "?mode=ro"
    with closing(sqlite3.connect(uri, uri=True)) as db:
        archive = Packages(db)
        for path in args.keys:
            keys = tuple(dict.fromkeys(path.read_text().split()))
            print(json.dumps({"input": path.name} | measure(archive, keys)), flush=True)


if __name__ == "__main__":
    main()
