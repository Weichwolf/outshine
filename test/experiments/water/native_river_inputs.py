"""Read the hash-pinned historical WATER2 reference; never use the WATER4 codec."""

import hashlib
import struct
from pathlib import Path

import numpy as np

SURFACE = np.dtype([("first_ring", "<u4"), ("ring_count", "<u4"), ("level_m", "<f4")])
RING = np.dtype([("first_point", "<u4"), ("point_count", "<u4")])
COURSE = np.dtype(
    [
        ("first_point", "<u4"),
        ("point_count", "<u4"),
        ("first_level", "<u4"),
        ("half_width_m", "<f4"),
    ]
)
TILE = np.dtype(
    [
        ("zoom", "<i4"),
        ("x", "<i4"),
        ("y", "<i4"),
        ("first_surface", "<u4"),
        ("surface_count", "<u4"),
    ]
)


class Reader:
    def __init__(self, data, label="WATER2"):
        self.data, self.offset, self.label = data, 0, label

    def number(self, code):
        size = struct.calcsize("<" + code)
        if self.offset + size > len(self.data):
            raise ValueError(f"truncated {self.label} scalar")
        value = struct.unpack_from("<" + code, self.data, self.offset)[0]
        self.offset += size
        return value

    def records(self, dtype, count_code="Q"):
        count = self.number(count_code)
        dtype = np.dtype(dtype)
        if count > (len(self.data) - self.offset) // dtype.itemsize:
            raise ValueError(f"{self.label} array exceeds its byte range")
        result = np.frombuffer(self.data, dtype=dtype, count=count, offset=self.offset)
        self.offset += result.nbytes
        return result


def within(first, count, end):
    return np.all(first.astype(np.uint64) + count.astype(np.uint64) <= end)


def source_reader(path, expected_sha256, label, magic, byte_limit):
    path = Path(path)
    if type(byte_limit) is not int or byte_limit <= 0:
        raise ValueError("invalid historical source byte budget")
    with path.open("rb") as source:
        data = source.read(byte_limit + 1)
    if len(data) > byte_limit:
        raise ValueError("historical source byte budget exceeded")
    if hashlib.sha256(data).hexdigest() != expected_sha256:
        raise ValueError("historical source digest mismatch")
    reader = Reader(data, label)
    if reader.number("Q") != magic:
        raise ValueError(
            f"historical {label} required; other codecs are not interchangeable"
        )
    return reader


def geometry(reader, surface_dtype=SURFACE, course_dtype=COURSE):
    """Shared historical geometry fields; version-specific metadata and EOF remain separate."""
    processed = reader.number("I")
    counters = [reader.number("q") for _ in range(3)]
    surfaces, rings, courses = (
        reader.records(dtype) for dtype in (surface_dtype, RING, course_dtype)
    )
    levels, scalars = reader.records("<f4", "I"), reader.records("<f8", "I")
    tiles = reader.records(TILE)
    if len(scalars) % 2:
        raise ValueError(f"{reader.label} incomplete point pairs")
    points = scalars.reshape(-1, 2)
    if (
        min(counters) < 0
        or processed > len(tiles)
        or not np.isfinite(points).all()
        or not np.isfinite(levels).all()
        or np.any(abs(points[:, 0]) > 90)
        or np.any(abs(points[:, 1]) > 180)
        or not np.isfinite(surfaces["level_m"]).all()
        or np.any(surfaces["ring_count"] == 0)
        or not within(surfaces["first_ring"], surfaces["ring_count"], len(rings))
        or np.any(rings["point_count"] < 3)
        or not within(rings["first_point"], rings["point_count"], len(points))
        or np.any(courses["point_count"] < 2)
        or not within(courses["first_point"], courses["point_count"], len(points))
        or not within(courses["first_level"], courses["point_count"], len(levels))
        or not np.isfinite(courses["half_width_m"]).all()
        or np.any(courses["half_width_m"] <= 0)
        or np.any((tiles["zoom"] < 0) | (tiles["zoom"] > 31))
        or not within(tiles["first_surface"], tiles["surface_count"], len(surfaces))
    ):
        raise ValueError(f"invalid {reader.label} ranges, coordinates or counters")
    across = np.left_shift(np.uint64(1), tiles["zoom"].astype(np.uint64))
    if np.any(
        (tiles["x"] < 0)
        | (tiles["y"] < 0)
        | (tiles["x"] >= across)
        | (tiles["y"] >= across)
    ):
        raise ValueError(f"invalid {reader.label} tile address")
    return {
        "surfaces": surfaces,
        "rings": rings,
        "courses": courses,
        "levels": levels,
        "points_lat_lon": points,
        "tiles": tiles,
        "processed_tiles": processed,
        "counters": counters,
    }


def read_v2(path, expected_sha256, *, byte_limit=1 << 20):
    """Return immutable array views of the complete original, including all sources."""
    reader = source_reader(path, expected_sha256, "WATER2", 0x325245544157, byte_limit)
    data = geometry(reader)
    if reader.offset != len(reader.data):
        raise ValueError("WATER2 trailing bytes")
    return dict(data, source_bytes=len(reader.data), consumed_bytes=reader.offset)


def surface_rings(data, index):
    """Preserve native ring order and holes; output coordinates remain latitude,longitude."""
    row = data["surfaces"][index]
    return [
        data["points_lat_lon"][r["first_point"] : r["first_point"] + r["point_count"]]
        for r in data["rings"][
            row["first_ring"] : row["first_ring"] + row["ring_count"]
        ]
    ]
