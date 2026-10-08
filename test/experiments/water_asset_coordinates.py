"""Measure source-independent water contour storage in existing native region packages.

Range remapping preserves source order, holes and flow profiles. This models coordinate
ownership; it does not measure native loading, allocator overhead or GPU performance.
"""
import argparse
import bisect
import json
import sqlite3
import struct
import subprocess
import zlib
from pathlib import Path


class Reader:
    def __init__(self, data):
        self.data = data
        self.at = 0

    def read(self, format):
        value = struct.unpack_from("<" + format, self.data, self.at)
        self.at += struct.calcsize("<" + format)
        return value

    def records(self, format):
        count, = self.read("Q")
        return [self.read(format) for _ in range(count)]


def remap(ranges):
    merged = []
    for first, count in sorted(ranges):
        end = first + count
        if merged and first <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], end))
        else:
            merged.append((first, end))
    mapping = {}
    offset = 0
    for first, end in merged:
        for original in range(first, end):
            mapping[original] = offset + original - first
        offset += end - first
    for first, count in ranges:
        assert all(mapping[first + at] == mapping[first] + at for at in range(count))
    assert list(mapping) == sorted(mapping)
    starts = [first for first, _ in merged]
    offsets = []
    offset = 0
    for first, end in merged:
        offsets.append(offset)
        offset += end - first
    for first, count in ranges:
        slot = bisect.bisect_right(starts, first) - 1
        assert slot >= 0 and first + count <= merged[slot][1]
        assert offsets[slot] + first - starts[slot] == mapping[first]
    return merged, mapping


def inspect(row):
    key, stored, crc, codec, native_bytes, offset, count = row
    assert zlib.crc32(stored) == crc
    native = stored if codec == 0 else subprocess.run(
        ["zstd", "-dc"], input=stored, capture_output=True, check=True).stdout
    assert codec in (0, 1) and len(native) == (native_bytes or len(stored))
    package = Reader(memoryview(native)[offset:offset + count])
    version, ground_bytes = package.read("QQ")
    assert version == 0x35524e474f
    package.at += ground_bytes
    water_bytes, = package.read("Q")
    water = Reader(package.data[package.at:package.at + water_bytes])
    version, source_scalars, tiles, no_ground, outliers, invalid = water.read("QQQqqq")
    assert version == 0x315245544157 and source_scalars % 2 == 0
    surfaces = water.records("IIf")
    rings = water.records("II")
    courses = water.records("IIIf")
    levels, = water.read("I")
    water.at += levels * 4
    memberships = [water.read("II") for _ in range(tiles)]
    assert water.at == water_bytes and package.at + water_bytes == len(package.data)
    assert all(first + count <= len(rings) for first, count, _ in surfaces)
    assert all(first + count <= len(surfaces) for first, count in memberships)
    assert all(level + count <= levels for _, count, level, _ in courses)
    ranges = rings + [(first, count) for first, count, _, _ in courses]
    assert all(count > 0 and first + count <= source_scalars // 2 for first, count in ranges)
    merged, mapping = remap(ranges)
    return dict(key=key, surfaces=len(surfaces), rings=len(rings), courses=len(courses),
                source_coordinate_bytes=source_scalars * 8, compact_coordinate_bytes=len(mapping) * 16,
                copied_contour_bytes=sum(count for _, count in ranges) * 16,
                contiguous_ranges=len(merged), water_package_bytes=water_bytes,
                estimated_independent_package_bytes=water_bytes + len(mapping) * 16 + tiles * 12,
                remap_dictionary_entries=len(mapping), remap_interval_entries=len(merged))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cache", type=Path)
    parser.add_argument("--packages", type=int, default=3)
    args = parser.parse_args()
    remap([(20, 4), (2, 3), (21, 2), (5, 2), (40, 2)])
    remap([])
    with sqlite3.connect(args.cache.resolve().as_uri() + "?mode=ro", uri=True) as database:
        rows = database.execute(
            "SELECT a.key,p.bytes,p.crc,p.codec,p.native_bytes,a.offset,a.bytes "
            "FROM assets a JOIN packages p ON a.package=p.key "
            "WHERE a.kind='ground-region' ORDER BY a.key LIMIT ?", (args.packages,))
        for row in rows:
            print(json.dumps(inspect(row)), flush=True)


if __name__ == "__main__":
    main()
