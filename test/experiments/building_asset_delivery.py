#!/usr/bin/env python3
"""Measure source-independent delivery from real schema-4 building assets, read-only."""
import argparse
from collections import Counter
import ctypes
import hashlib
import json
from pathlib import Path
import sqlite3
import struct
import subprocess

from prepared_building_residency import Reader, unpack


def library():
    under = subprocess.check_output(['pkg-config', '--variable=libdir', 'libzstd'], text=True).strip()
    candidates = [Path(under) / name for name in ('libzstd.dylib', 'libzstd.so')]
    lib = ctypes.CDLL(str(next(path for path in candidates if path.is_file())))
    lib.ZSTD_decompress.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p,
                                   ctypes.c_size_t]
    lib.ZSTD_decompress.restype = ctypes.c_size_t
    lib.ZSTD_isError.argtypes = [ctypes.c_size_t]
    lib.ZSTD_isError.restype = ctypes.c_uint
    return lib


def ranges_used(ranges, points):
    merged = []
    for first, end in sorted(ranges):
        if not 0 <= first <= end <= len(points) // 16:
            raise ValueError('coordinate range exceeds its native asset')
        if merged and first <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], end)
        else:
            merged.append([first, end, 0])
    packed = 0
    for row in merged:
        row[2] = packed
        packed += row[1] - row[0]
    compact = b''.join(points[first * 16:end * 16] for first, end, _ in merged)
    for first, end in ranges:
        row = next(row for row in merged if row[0] <= first < row[1])
        remapped = row[2] + first - row[0]
        if compact[remapped * 16:(remapped + end - first) * 16] != points[first * 16:end * 16]:
            raise ValueError('packing changed an ordered ring')
    return len(compact), len(merged)


def inspect(data):
    r = Reader(data)
    if (r.number('I'), r.number('I')) != (0x31425350, 4):
        raise ValueError('experiment requires native building index schema 4')
    r.skip(24 + 8 + 4 + 1 + 8 + 4 + 32 + 8)
    if r.flag():
        r.text()
        r.text()
        for _ in range(r.number('Q')):
            r.text()
        if r.flag():
            r.skip(12)
    for _ in range(r.number('Q')):
        r.skip(4 + 1 + 12)
        if r.flag():
            r.skip(8)
        r.text()
        r.text()
    r.skip(8 + 1 + 4 + 1)
    r.fixed_list(12)
    header = r.at
    coordinates_start = r.at
    doubles = r.number('Q')
    start = r.at
    r.skip(doubles * 8)
    points = data[start:r.at]
    coordinates = data[coordinates_start:r.at]
    holes_start = r.at
    holes = [(r.number('I'), r.number('I'), r.number('B')) for _ in range(r.number('Q'))]
    holes_bytes = r.at - holes_start
    hole_wire = data[holes_start:r.at]
    start = r.at
    r.fixed_list(8)
    corner_bytes = r.at - start
    start = r.at
    layouts, cells, sources = [], Counter(), []
    for _ in range(r.number('Q')):
        layouts.append(struct.unpack_from('<6I', data, r.at))
        source = Reader(data)
        source.at = r.at
        source.skip(24 + 4 + 32 + 16 + 4)
        if source.flag():
            source.skip(12)
        if source.flag():
            source.skip(1)
        identity = data[source.at:source.at + 9]
        cell, _ = r.prepared()
        sources.append(struct.pack('<I', cell) + identity)
        cells[cell] += 1
    plans_bytes = r.at - start
    start = r.at
    if r.number('Q') != len(layouts):
        raise ValueError('surface summaries do not cover all plans')
    for _ in layouts:
        if r.flag():
            r.skip(4 * 24 + 48 + 8)
            if r.flag():
                r.skip(12)
            if r.flag():
                r.skip(48)
            if r.flag():
                r.skip(8)
            r.skip(4 + 1)
    if r.at != len(data):
        raise ValueError('native wire layout or trailing bytes changed')
    summaries_bytes = r.at - start
    ranges = []
    for first, count, _, hole, hole_count, _ in layouts:
        ranges.append((first, first + count))
        for offset, length, _ in holes[hole:hole + hole_count]:
            ranges.append((offset, offset + length))
    compact, intervals = ranges_used(ranges, points)
    basis = data[:header] + coordinates + hole_wire + struct.pack('<Q', len(sources)) + b''.join(sources)
    return dict(native_bytes=len(data), structures=len(layouts), occupied_cells=len(cells),
                header_bytes=header, coordinate_bytes=len(points), holes_bytes=holes_bytes,
                corner_bytes=corner_bytes, plan_bytes=plans_bytes, summary_bytes=summaries_bytes,
                compact_coordinate_bytes=compact, coordinate_intervals=intervals,
                ordered_rings_bit_identical=True, source_independent_basis_model_bytes=len(basis),
                basis_model_sha256=hashlib.sha256(basis).hexdigest(),
                basis_usage_assumption='cached LOD restores coordinates, origin, source IDs and cells')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--largest', type=int, default=3)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.largest < 1:
        parser.error('--largest must be positive')
    lib, rows = library(), []
    with sqlite3.connect(args.cache.resolve().as_uri() + '?mode=ro', uri=True) as db:
        query = """SELECT a.key,p.codec,p.native_bytes,p.bytes FROM assets a
                   JOIN packages p ON p.key=a.package WHERE a.kind='buildings' AND a.parent=''
                   ORDER BY a.bytes DESC LIMIT ?"""
        for key, codec, size, wire in db.execute(query, (args.largest,)):
            data = unpack(lib, wire, codec, size)
            rows.append(dict(key=key, native_sha256=hashlib.sha256(data).hexdigest(), **inspect(data)))
    if len(rows) != args.largest:
        raise ValueError('requested sample exceeds available complete building roots')
    report = dict(cache=str(args.cache.resolve()), schema=4, scope='largest complete building roots',
                  cpu_gpu_ram_gain_proven=False, native_delivery_integration_proven=False, rows=rows)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    for row in rows:
        print(json.dumps(row, sort_keys=True))


if __name__ == '__main__':
    main()
