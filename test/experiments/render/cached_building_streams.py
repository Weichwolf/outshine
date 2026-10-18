"""Read-only schema-10 geometry streams and schema-6 source identities for #567."""

import hashlib
import struct
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "transport"))
from prepared_building_residency import Reader

STREAM_BYTES = (24, 24, 4, 4, 52, 4, 4, 4, 52, 4)


def array(reader, item_bytes):
    count = reader.number("Q")
    start = reader.at
    reader.skip(count * item_bytes)
    return memoryview(reader.data)[start : reader.at], count


def product(data, key):
    if not 149 <= len(data) <= 64 * 1024**2 or not data.startswith(
        b"outshine-structure"
    ):
        raise ValueError("bounded native structure artifact required")
    if hashlib.sha256(data[:-64]).hexdigest().encode() != data[-64:]:
        raise ValueError("structure artifact checksum mismatch")
    r = Reader(data[:-64])
    r.skip(18)
    if r.number("I") != 10:
        raise ValueError("explicitly supported structure schema 10 required")
    r.skip(64)
    if data[22:86].decode("ascii") != key:
        raise ValueError("structure input key mismatch")
    streams, counts = zip(*(array(r, width) for width in STREAM_BYTES), strict=True)
    digest, fallback = r.number("Q"), r.flag()
    view = None
    if r.flag():
        start = r.at
        longitude, latitude = r.number("d"), r.number("d")
        eye = tuple(r.number("d") for _ in range(3)) if r.flag() else None
        focal, error, radius = (r.number("d") for _ in range(3))
        view = {
            "input_bytes": bytes(data[start : r.at - 8]).hex(),
            "longitude": longitude,
            "latitude": latitude,
            "eye_ecef": eye,
            "focal_px": focal,
            "error_px": error,
            "radius_m": radius,
        }
    detail = r.number("B") if r.flag() else None
    cell = r.number("I") if r.flag() else None
    if r.flag():
        r.skip(32)
    occupied = r.number("Q")
    r.skip(64 * (32 + 4 + 8))
    footprints = r.fixed_list(86)
    if any(r.fixed_list(width) != footprints for width in (1, 8, 8)):
        raise ValueError("footprint metadata length mismatch")
    counters = [r.number("i") for _ in range(6)] + [r.number("Q"), r.number("Q")]
    if r.flag():
        r.skip(32)
    if r.flag():
        r.skip(4)
    if r.at != len(data) - 64:
        raise ValueError("unexpected schema fields or trailing bytes")
    validate_streams(streams, counts)
    return {
        "key": key,
        "streams": streams,
        "counts": counts,
        "view": view,
        "cell": cell,
        "detail": detail,
        "footprints": footprints,
        "counters": counters,
        "occupied": occupied,
        "digest": digest,
        "fallback": fallback,
    }


def validate_streams(streams, counts):
    if counts[5] not in (0, counts[0] * 4) or counts[6] not in (0, counts[0] * 2):
        raise ValueError("optional vertex attribute length mismatch")
    for stream, vertices in ((2, 0), (3, 1), (7, 0), (9, 1)):
        indices = np.frombuffer(streams[stream], dtype="<u4")
        if len(indices) % 3 or (
            len(indices) and int(indices.max()) >= counts[vertices]
        ):
            raise ValueError("invalid native triangle indices")
    for stream, indices in ((4, 7), (8, 9)):
        records = np.frombuffer(streams[stream], dtype="<u4").reshape(-1, 13)
        first, size = records[:, 10].astype(np.uint64), records[:, 11].astype(np.uint64)
        if ((first + size > counts[indices]) | (size % 3 != 0)).any():
            raise ValueError("cluster range escapes its source index stream")


def basis_sources(data):
    r = Reader(data)
    if (r.number("I"), r.number("I")) != (0x31425350, 6):
        raise ValueError("explicitly supported building basis schema 6 required")
    anchor = struct.unpack_from("<3d", data, r.at)
    r.skip(24 + 8 + 4 + 1 + 8 + 4 + 32 + 8)
    if r.flag():
        r.text()
        r.text()
        for _ in range(r.number("Q")):
            r.text()
        if r.flag():
            r.skip(12)
    for _ in range(r.number("Q")):
        r.skip(4 + 1 + 12)
        if r.flag():
            r.skip(8)
        r.text()
        r.text()
    r.skip(8 + 1 + 4 + 1)
    r.fixed_list(12)
    r.fixed_list(8)
    r.fixed_list(9)
    sources, _ = array(r, 13)
    if r.at != len(data):
        raise ValueError("unexpected basis fields or trailing bytes")
    records = np.frombuffer(
        sources, dtype=[("cell", "<u4"), ("id", "<u8"), ("kind", "u1")]
    )
    if ((records["cell"] < 1) | (records["cell"] > 64)).any():
        raise ValueError("source cell outside the native 8×8 grid")
    return anchor, records
