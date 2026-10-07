#!/usr/bin/env python3
"""Measure real native building packages without changing the cache.

Wire format: PreparedStructureCodec version 3, with 64-bit size_t/long.
The spatial block model measures
independent shape reads; it does not prove visibility, LOD quality or frame time.
"""
import argparse
from collections import Counter
import ctypes
import ctypes.util
import hashlib
import json
import math
from pathlib import Path
import sqlite3
import struct
import time
import zlib


class Reader:
    def __init__(self, data):
        self.data, self.at = data, 0

    def skip(self, size):
        if size < 0 or size > len(self.data) - self.at:
            raise ValueError("truncated native building package")
        self.at += size

    def number(self, code):
        size = struct.calcsize('<' + code)
        self.skip(size)
        return struct.unpack_from('<' + code, self.data, self.at - size)[0]

    def flag(self):
        value = self.number('B')
        if value > 1:
            raise ValueError("invalid presence flag")
        return value

    def text(self):
        self.skip(self.number('Q'))

    def fixed_list(self, size):
        count = self.number('Q')
        self.skip(count * size)
        return count

    def shape(self):
        vertices = self.fixed_list(16)
        for _ in range(self.number('Q')):
            vertices += self.fixed_list(16)
        self.skip(8)
        self.fixed_list(1)
        self.skip(64 + 2 + 4 + 11 * 8 + 4 * 4)
        return vertices

    def surface(self):
        begin = self.at
        if not self.flag():
            return begin, self.at - begin, 0, None
        self.skip(4 * 24)
        box = struct.unpack_from('<6d', self.data, self.at)
        self.skip(48 + 8)
        if self.flag():
            self.skip(12)
        vertices = sum(self.shape() for _ in range(self.number('Q')))
        self.fixed_list(8)
        return begin, self.at - begin, vertices, box

    def prepared(self):
        self.skip(6 * 4)
        cell = self.number('I')
        self.skip(32 + 16 + 4)
        if self.flag():
            self.skip(12)
        if self.flag():
            self.skip(1)
        self.skip(9 + 4 * 4)
        height, _, base, seat, foot = struct.unpack_from('<5f', self.data, self.at)
        self.skip(5 * 4 + 1 + 1 + 6 * 8)
        bounds = struct.unpack_from('<4d', self.data, self.at)
        self.skip(32 + 8 + 4 * 8)
        top = struct.unpack('<f', struct.pack('<f', seat + height))[0]
        return cell, (*bounds, min(base, foot), top)


def inspect(data):
    r = Reader(data)
    magic, version = r.number('I'), r.number('I')
    if (magic, version) != (0x31425350, 3):
        return None
    anchor = struct.unpack_from('<3d', data, r.at)
    r.skip(24 + 8 + 4 + 1 + 8 + 4)
    coverage = struct.unpack_from('<4d', data, r.at)
    r.skip(32 + 8)
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
    r.fixed_list(20)
    parts = Counter(metadata=r.at)
    for name, size in [('coordinates', 8), ('holes', 9), ('corner_heights', 8)]:
        begin = r.at
        r.fixed_list(size)
        parts[name] = r.at - begin
    begin = r.at
    records = [r.prepared() for _ in range(r.number('Q'))]
    parts['prepared_records'] = r.at - begin
    begin = r.at
    surfaces = [r.surface() for _ in range(r.number('Q'))]
    parts['prepared_shapes'] = r.at - begin
    if r.at != len(data) or len(records) != len(surfaces):
        raise ValueError(f"wire layout mismatch at {r.at}/{len(data)}")
    assert sum(parts.values()) == len(data)
    return anchor, coverage, records, surfaces, parts


def ecef(lon, lat, height):
    lon, lat = math.radians(lon), math.radians(lat)
    eccentricity = 0.0066943799901413165
    n = 6378137 / math.sqrt(1 - eccentricity * math.sin(lat)**2)
    return ((n + height) * math.cos(lat) * math.cos(lon),
            (n + height) * math.cos(lat) * math.sin(lon),
            (n * (1 - eccentricity) + height) * math.sin(lat))


def distance_to_box(eye, box):
    return math.sqrt(sum(max(box[i] - eye[i], eye[i] - box[i + 3], 0)**2 for i in range(3)))


def fallback_bounds(geographic):
    west, south, east, north, bottom, top = geographic
    centre = ecef((west + east) * 0.5, (south + north) * 0.5, (bottom + top) * 0.5)
    radius = ((6400000 + max(abs(bottom), abs(top))) * math.radians(east - west + north - south)
              * 0.5 + abs(top - bottom) * 0.5 + 1e-6)
    return [value - radius for value in centre] + [value + radius for value in centre]


def unpack(lib, stored, codec, native_size):
    if len(stored) > 64 * 1024**2 or native_size > 64 * 1024**2:
        raise ValueError('package exceeds native building byte budget')
    if codec == 0:
        if native_size not in (0, len(stored)):
            raise ValueError("invalid raw package size")
        return stored
    if codec != 1 or native_size > 64 * 1024**2:
        raise ValueError("unsupported package")
    into = ctypes.create_string_buffer(native_size)
    count = lib.ZSTD_decompress(into, native_size, stored, len(stored))
    if lib.ZSTD_isError(count) or count != native_size:
        raise ValueError("invalid zstd package")
    return into.raw


def compress(lib, data):
    into = ctypes.create_string_buffer(lib.ZSTD_compressBound(len(data)))
    count = lib.ZSTD_compress(into, len(into), data, len(data), 1)
    if lib.ZSTD_isError(count):
        raise ValueError('zstd compression failed')
    return min(len(data), count)


def check_blocks(data, root_size, cells, surfaces, chunks):
    digest = hashlib.sha256(data[:root_size])
    digest.update(struct.pack('<Q', len(surfaces)))
    cursors = Counter()
    for cell, (_, size, _, _) in zip(cells, surfaces):
        begin = cursors[cell]
        digest.update(chunks[cell][begin:begin + size])
        cursors[cell] += size
    if digest.digest() != hashlib.sha256(data).digest():
        raise ValueError('spatial blocks do not preserve the native package')
    if any(cursors[cell] != len(chunk) for cell, chunk in chunks.items()):
        raise ValueError('unreferenced shape block bytes')


def measure(args):
    library = args.zstd_library or ctypes.util.find_library('zstd')
    if not library:
        raise ValueError('libzstd not found; pass --zstd-library')
    lib = ctypes.CDLL(library)
    lib.ZSTD_decompress.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
                                   ctypes.c_void_p, ctypes.c_size_t]
    lib.ZSTD_decompress.restype = ctypes.c_size_t
    lib.ZSTD_isError.argtypes = [ctypes.c_size_t]
    lib.ZSTD_compressBound.argtypes = [ctypes.c_size_t]
    lib.ZSTD_compressBound.restype = ctypes.c_size_t
    lib.ZSTD_compress.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
                                ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]
    lib.ZSTD_compress.restype = ctypes.c_size_t
    db = sqlite3.connect(args.cache.resolve().as_uri() + '?mode=ro', uri=True)
    eye = ecef(*args.eye)
    records = db.execute("SELECT a.key,a.package,a.minx,a.miny,a.minz,a.maxx,a.maxy,a.maxz "
                         "FROM assets a WHERE kind='buildings' ORDER BY a.id DESC").fetchall()
    parts, blocks, block_bounds = Counter(), Counter(), {}
    totals = Counter()
    radii = [250, 1000, 4000, 16000]
    selected = {radius: Counter() for radius in radii}
    began = time.perf_counter()
    for key, package, *box in records:
        if distance_to_box(eye, box) > args.radius:
            continue
        stored, crc, codec, native_size = db.execute(
            'SELECT bytes,crc,codec,native_bytes FROM packages WHERE key=?', (package,)).fetchone()
        if zlib.crc32(stored) != crc:
            raise ValueError(f"package CRC mismatch: {key}")
        data = unpack(lib, stored, codec, native_size)
        parsed = inspect(data)
        if parsed is None:
            totals['old_schema_packages'] += 1
            continue
        anchor, coverage, records, surfaces, sizes = parsed
        cells = [record[0] for record in records]
        parts.update(sizes)
        totals.update(packages=1, compressed_bytes=len(stored), native_bytes=len(data),
                      structures=len(cells), shape_vertices=sum(s[2] for s in surfaces))
        root_size = len(data) - sizes['prepared_shapes']
        totals['eager_metadata_compressed_bytes'] += compress(lib, data[:root_size])
        boxes, shape_chunks = {}, {}
        for (cell, geographic), (offset, size, vertices, bounds) in zip(records, surfaces):
            blocks[(key, cell)] += size
            shape_chunks.setdefault(cell, bytearray()).extend(data[offset:offset + size])
            if bounds is None:
                totals['unsupported_shapes'] += 1
                world = fallback_bounds(geographic)
            else:
                world = [bounds[i] + anchor[i % 3] for i in range(6)]
            box = boxes.setdefault(cell, world.copy())
            for i in range(3):
                box[i] = min(box[i], world[i])
                box[i + 3] = max(box[i + 3], world[i + 3])
            distance = distance_to_box(eye, world)
            for radius in radii:
                if distance <= radius:
                    selected[radius].update(shape_bytes=size, structures=1, vertices=vertices)
        compressed = {cell: compress(lib, bytes(chunk)) for cell, chunk in shape_chunks.items()}
        check_blocks(data, root_size, cells, surfaces, shape_chunks)
        block_bounds.update(((key, cell), box) for cell, box in boxes.items())
        totals['shape_blocks_compressed_bytes'] += sum(compressed.values())
        for radius in radii:
            needed = [cell for cell, box in boxes.items() if distance_to_box(eye, box) <= radius]
            selected[radius]['cell_shape_bytes'] += sum(blocks[(key, cell)] for cell in needed)
            selected[radius]['cell_shape_compressed_bytes'] += sum(compressed[cell] for cell in needed)
    query_ms = []
    for _ in range(25):
        start = time.perf_counter()
        chosen = [key for key, box in block_bounds.items() if distance_to_box(eye, box) <= 1000]
        query_ms.append((time.perf_counter() - start) * 1000)
    return dict(eye=args.eye, radius_m=args.radius, totals=totals, parts_bytes=parts,
                cell_blocks=len(blocks), largest_cell_shape_bytes=max(blocks.values(), default=0),
                lossless_block_reassembly='sha256 verified for every inspected package',
                shape_block_bounds_bytes=len(block_bounds) * 48,
                python_bbox_query_1km_median_ms=round(sorted(query_ms)[12], 3),
                bbox_query_selected_cells=len(chosen),
                distance_only_model=selected, elapsed_s=round(time.perf_counter() - began, 3),
                qualification='All schema-3 packages intersecting radius; not exact runtime key set. '
                              'All existing plan/coordinate metadata still eager; insufficient as engine root. '
                              'Separate zstd level-1 shape blocks per cell. '
                              'Index/record overhead excluded from compressed model. '
                              'Distance thresholds are an ablation, not LOD error bounds.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cache', type=Path)
    parser.add_argument('--eye', type=float, nargs=3, required=True, metavar=('LON', 'LAT', 'ASL'))
    parser.add_argument('--radius', type=float, default=240000)
    parser.add_argument('--zstd-library')
    args = parser.parse_args()
    if not all(math.isfinite(value) for value in (*args.eye, args.radius)) or args.radius <= 0:
        parser.error('eye and radius must be finite; radius must be positive')
    if abs(args.eye[0]) > 180 or abs(args.eye[1]) > 90:
        parser.error('eye longitude/latitude out of range')
    print(json.dumps(measure(args), indent=2))


if __name__ == '__main__':
    main()
