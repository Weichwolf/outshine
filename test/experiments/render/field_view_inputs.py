"""Read one existing native material view, preserving its checksum and binding."""

import hashlib
import sqlite3
import struct

import numpy as np
from compare_vertex_field_words import Zstd
from field_view_sampling import FieldGrid

TEXEL = np.dtype([("normal", "<f4", 3), ("depth", "<f4"), ("surface", "<u4")])
MATERIAL = np.dtype([("colour", "<f4", 3), ("roughness", "<f4"), ("metal", "<f4")])


def fnv(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value


def decode_view(raw, key):
    if len(raw) < 96:
        raise ValueError("Truncated native material view")
    magic, version, binding, n, views, surfaces, *shape = struct.unpack_from(
        "<QIQIII4d", raw
    )
    if (
        magic != 0x004E574F5243534F
        or version != 2
        or binding != fnv(key.encode())
        or views != 1
        or not 3 <= n <= 512
        or not 1 <= surfaces <= 4096
        or len(raw) != 64 + 52 * surfaces + 24 + 40 * n * n + 8
        or struct.unpack_from("<Q", raw, len(raw) - 8)[0] != fnv(raw[:-8])
    ):
        raise ValueError(
            "Unsupported, corrupt or differently bound native material view"
        )
    at = 64 + 52 * surfaces
    direction = np.frombuffer(raw, dtype="<f8", count=3, offset=at)
    grid = FieldGrid(tuple(shape[:3]), tuple(direction), shape[3])
    texels = np.frombuffer(raw, dtype=TEXEL, count=n * n, offset=at + 24).reshape(n, n)
    materials = np.frombuffer(
        raw, dtype=MATERIAL, count=n * n, offset=at + 24 + 20 * n * n
    ).reshape(n, n)
    valid = texels["surface"] > 0
    normals, depth = texels["normal"], texels["depth"]
    if (
        not np.isfinite(normals).all()
        or not np.isfinite(depth).all()
        or np.any(texels["surface"] > surfaces)
        or np.any((depth > 0) != valid)
        or np.any((depth < 0) | (depth > 1))
        or np.any(normals[~valid] != 0)
        or np.any(abs(np.linalg.norm(normals[valid], axis=1) - 1) > 0.01)
        or not np.isfinite(materials.view("<f4")).all()
    ):
        raise ValueError("Invalid coupled depth, normal, coverage or material samples")
    return {
        "grid": grid,
        "pixels": n,
        "texels": texels,
        "materials": materials,
        "raw_bytes": len(raw),
        "sha256": hashlib.sha256(raw).hexdigest(),
        "key": key,
    }


def read_view(path, key):
    with sqlite3.connect(path.resolve().as_uri() + "?mode=ro", uri=True) as db:
        row = db.execute(
            "SELECT a.kind,a.offset,a.bytes,p.codec,p.native_bytes,length(p.bytes),p.key "
            "FROM assets a JOIN packages p ON a.package=p.key WHERE a.key=?",
            (key,),
        ).fetchone()
        if row is None:
            raise ValueError("The bound native view is missing")
        kind, start, count, codec, size, wire_size, package = row
        if (
            kind != "impostor-material-view-1"
            or codec not in (0, 1)
            or not 0 < size <= 64 * 1024 * 1024
            or not 0 < wire_size <= 64 * 1024 * 1024
            or start < 0
            or count <= 0
            or start + count > size
        ):
            raise ValueError("Unsupported or unbounded native material package")
        blob = db.execute(
            "SELECT bytes FROM packages WHERE key=?", (package,)
        ).fetchone()[0]
    raw = Zstd().decompress((codec, blob), size)
    return decode_view(raw[start : start + count], key)
