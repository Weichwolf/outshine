#!/usr/bin/env python3
"""Spatial asset-package experiment on cached MVT plans, not a runtime asset format.

Compare SQLite object/package R*Trees with packed-package scans. All candidates are
refined against the same double bounds. Warm loading only reads prepared arrays;
terrain contacts, native roofs/materials, meshing and GPU upload are not modelled.
"""
import argparse
import io
import json
import math
import sqlite3
import time
import zlib
from pathlib import Path

import numpy as np

from building_lod import read_plans


def elapsed(start):
    return (time.perf_counter() - start) * 1000


def median_ms(operation, repeats=15):
    measured = []
    for _ in range(repeats):
        start = time.perf_counter()
        operation()
        measured.append(elapsed(start))
    return float(np.median(measured))


def prepare(plans, size):
    bounds, vertices, rings, objects, cursor, ring_cursor = [], [], [], [], 0, 0
    for footprint, top, bottom, colour, centre in plans:
        outer = footprint[0]
        bounds.append([*outer.min(axis=0), bottom, *outer.max(axis=0), top])
        objects.append([ring_cursor, len(footprint), top, bottom, *colour])
        for ring in footprint:
            rings.append([cursor, len(ring)])
            vertices.extend(ring)
            cursor += len(ring)
            ring_cursor += 1
    bounds = np.asarray(bounds, dtype=np.float64)
    cells = np.floor((bounds[:, :2] + bounds[:, 3:5]) * .5 / size).astype(np.int64)
    _, owners = np.unique(cells, axis=0, return_inverse=True)
    order = np.argsort(owners, kind='stable')
    begins = np.r_[0, np.flatnonzero(np.diff(owners[order])) + 1]
    ends = np.r_[begins[1:], len(order)]
    packages = np.c_[np.minimum.reduceat(bounds[order, :3], begins),
                     np.maximum.reduceat(bounds[order, 3:], begins)]
    return dict(bounds=bounds, vertices=np.asarray(vertices, dtype=np.float64),
                rings=np.asarray(rings, dtype=np.uint32),
                objects=np.asarray(objects, dtype=np.float64), order=order,
                begins=begins, ends=ends, packages=packages)


def intersect(bounds, query):
    return np.all(bounds[:, :3] <= query[3:], axis=1) & np.all(bounds[:, 3:] >= query[:3], axis=1)


def exact(bounds, query, centre, radius, planes):
    inside = intersect(bounds, query)
    nearest = np.maximum(bounds[:, :3] - centre, np.maximum(centre - bounds[:, 3:], 0))
    inside &= np.sum(nearest * nearest, axis=1) <= radius * radius
    for normal, offset in planes:
        support = np.where(normal >= 0, bounds[:, 3:], bounds[:, :3])
        inside &= np.sum(support * normal, axis=1) + offset >= 0
    return inside


def database(path, objects, packages):
    if path.exists():
        path.unlink()
    connection = sqlite3.connect(path)
    start = time.perf_counter()
    for name, bounds in [('objects', objects), ('packages', packages)]:
        connection.execute(f'CREATE VIRTUAL TABLE {name} USING rtree(id, x0, x1, y0, y1, z0, z1)')
        connection.executemany(f'INSERT INTO {name} VALUES(?,?,?,?,?,?,?)',
                               ((i, row[0], row[3], row[1], row[4], row[2], row[5])
                                for i, row in enumerate(bounds)))
    connection.commit()
    return connection, elapsed(start)


def query_tree(connection, table, query):
    return np.fromiter((row[0] for row in connection.execute(
        f'SELECT id FROM {table} WHERE x0<=? AND x1>=? AND y0<=? AND y1>=? AND z0<=? AND z1>=?',
        (query[3], query[0], query[4], query[1], query[5], query[2]))), dtype=np.int64)


def expand(arrays, packages):
    return np.concatenate([arrays['order'][arrays['begins'][p]:arrays['ends'][p]]
                           for p in packages]) if len(packages) else np.empty(0, dtype=np.int64)


def cases():
    for centre in [np.array([0., 0., 250.]), np.array([1000., -1500., 20.])]:
        for radius in [100., 1000., 5000., 240000.]:
            yield f'radius-{radius:g}@{centre[0]:g}', centre, radius, []
    centre = np.array([0., 0., 250.])
    # Frustum sides through the eye, near/far along horizontal viewing direction.
    for yaw in range(0, 360, 45):
        angle = math.radians(yaw)
        forward = np.array([math.sin(angle), math.cos(angle), 0.])
        right = np.array([math.cos(angle), -math.sin(angle), 0.])
        up = np.array([0., 0., 1.])
        normals = [forward + right, forward - right,
                   forward * .5625 + up, forward * .5625 - up]
        planes = [(normal, -normal @ centre) for normal in normals]
        planes.extend([(forward, -forward @ centre - .1),
                       (-forward, forward @ centre + 10000.)])
        yield f'frustum-{yaw}', centre, 10000., planes


def benchmark(arrays, connection):
    bounds = arrays['bounds']
    results = []
    for name, centre, radius, planes in cases():
        query = np.r_[centre - radius, centre + radius]
        reference = np.flatnonzero(exact(bounds, query, centre, radius, planes))
        def packages(ids):
            ids = ids[exact(arrays['packages'][ids], query, centre, radius, planes)]
            return expand(arrays, ids)
        methods = {
            'scan_objects': lambda: np.arange(len(bounds)),
            'rtree_objects': lambda: query_tree(connection, 'objects', query),
            'rtree_packages': lambda: packages(query_tree(connection, 'packages', query)),
            'scan_packages': lambda: packages(np.flatnonzero(intersect(arrays['packages'], query)))
        }
        measured = {}
        for method, candidates in methods.items():
            def select():
                ids = candidates()
                return ids[exact(bounds[ids], query, centre, radius, planes)]
            selected = select()
            assert np.array_equal(np.sort(selected), reference), (name, method)
            measured[method] = dict(ms=median_ms(select), candidates=len(candidates()))
        results.append(dict(case=name, selected=len(reference), methods=measured))
    return results


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--longitude', type=float, required=True)
    parser.add_argument('--latitude', type=float, required=True)
    parser.add_argument('--package-size', type=float, default=256.)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not math.isfinite(args.package_size) or args.package_size <= 0:
        parser.error('package-size must be finite and positive')
    args.output.mkdir(parents=True, exist_ok=True)
    start = time.perf_counter()
    plans, source_bytes, missing = read_plans(args.manifest, (args.longitude, args.latitude))
    source_ms = elapsed(start)
    start = time.perf_counter()
    arrays = prepare(plans, args.package_size)
    assert all(np.isfinite(value).all() for value in arrays.values())
    prepare_ms = elapsed(start)
    path = args.output / 'prepared-plans.npz'
    np.savez(path, **arrays)
    def load():
        with np.load(path, allow_pickle=False) as archive:
            return {key: archive[key] for key in archive.files}
    warm_ms = median_ms(load, repeats=5)
    loaded = load()
    assert all(np.array_equal(value, loaded[key]) for key, value in arrays.items())
    connection, index_ms = database(args.output / 'spatial.sqlite', arrays['bounds'], arrays['packages'])
    payload = path.read_bytes()
    checksum = zlib.crc32(payload)
    connection.execute('CREATE TABLE payloads(id INTEGER PRIMARY KEY, bytes BLOB, crc INTEGER)')
    start = time.perf_counter()
    connection.execute('INSERT INTO payloads VALUES(1,?,?)', (payload, checksum))
    connection.commit()
    blob_write_ms = elapsed(start)
    def load_blob():
        data, checksum = connection.execute('SELECT bytes,crc FROM payloads WHERE id=1').fetchone()
        assert zlib.crc32(data) == checksum
        with np.load(io.BytesIO(data), allow_pickle=False) as archive:
            return {key: archive[key] for key in archive.files}
    def load_file_checked():
        data = path.read_bytes()
        assert zlib.crc32(data) == checksum
        with np.load(io.BytesIO(data), allow_pickle=False) as archive:
            return {key: archive[key] for key in archive.files}
    restored = load_blob()
    assert all(np.array_equal(value, restored[key]) for key, value in arrays.items())
    result = dict(model='prepared local flat building plans; not native game assets',
                  polygons=len(plans), packages=len(arrays['packages']),
                  source_bytes=source_bytes, supplied_height_fallbacks=missing,
                  source_decode_ms=source_ms, prepare_ms=prepare_ms,
                  prepared_bytes=path.stat().st_size, warm_read_ms=warm_ms,
                  sqlite_blob_write_ms=blob_write_ms, sqlite_blob_read_checked_ms=median_ms(load_blob, 5),
                  file_read_checked_ms=median_ms(load_file_checked, 5),
                  index_build_ms=index_ms, sqlite_version=sqlite3.sqlite_version,
                  package_size_m=args.package_size, queries=benchmark(loaded, connection))
    connection.close()
    (args.output / 'costs.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: value for key, value in result.items() if key != 'queries'}))
    for row in result['queries']:
        print(row['case'], row['selected'],
              ' '.join(f'{name}={cost["ms"]:.3f}ms/{cost["candidates"]}'
                       for name, cost in row['methods'].items()))


if __name__ == '__main__':
    main()
