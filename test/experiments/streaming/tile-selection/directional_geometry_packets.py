"""Replay direction-indexed opaque geometry on a recorded camera path.

Counts only the main pass for opaque, front-facing triangles. Each triangle
belongs to one packet. An unread packet must face away across the entire eye
box. Disk compression, shadow demand, GPU timing and publication are separate.
"""

import argparse
import json
import sqlite3
import xml.etree.ElementTree as ET
from collections import Counter
from contextlib import closing
from itertools import product
from pathlib import Path
from time import perf_counter

import numpy as np
from building_shadow_packets import Packages, geometry
from tile_selection import camera_path, local_frame


def directions(subdivisions):
    if subdivisions == 0:
        return np.vstack((-np.eye(3), np.eye(3)))
    values = range(-subdivisions, subdivisions + 1)
    vectors = np.asarray(list(product(values, repeat=3)))
    shell = vectors[np.max(np.abs(vectors), axis=1) == subdivisions]
    return shell / np.linalg.norm(shell, axis=1)[:, None]


def upper_support(eye, normal_low, normal_high, offset):
    # Maximise each independent product over two intervals. Correlations
    # discarded by the packet bounds can only retain unnecessary triangles.
    products = (
        eye[0] * normal_low,
        eye[0] * normal_high,
        eye[1] * normal_low,
        eye[1] * normal_high,
    )
    high = np.maximum.reduce(products)
    high = np.nextafter(high, np.inf)
    horizontal = np.nextafter(high[..., 0] + high[..., 1], np.inf)
    spatial = np.nextafter(horizontal + high[..., 2], np.inf)
    return np.nextafter(spatial + offset, np.inf)


def measure_surface(words, indices, centre, axes, eye, banks):
    vertices = words.view("<f4").astype(np.float64) - centre
    positions = np.einsum("ij,jk->ik", vertices, axes)
    faces = indices.reshape(-1, 3)
    if len(faces) == 0:
        return [Counter() for _ in banks]
    vertices = positions[faces]
    edge_a, edge_b = vertices[:, 1] - vertices[:, 0], vertices[:, 2] - vertices[:, 0]
    facing = np.cross(edge_a, edge_b)
    length = np.linalg.norm(facing, axis=1)
    normal = facing / np.where(length > 0, length, 1)[:, None]
    offset = -np.einsum("ni,ni->n", normal, vertices[:, 0])
    if not np.isfinite(normal).all() or not np.isfinite(offset).all():
        raise ValueError("building face has invalid coordinates")
    extent = 1 + np.max(np.abs(positions), axis=0).sum()
    extent += np.max(np.abs(eye), axis=0).sum()
    tolerance_m = 64 * np.finfo(np.float32).eps * extent
    potential = upper_support(eye, normal, normal, offset) >= -tolerance_m
    result = []
    for bank in banks:
        partition = np.argmax(np.einsum("ni,mi->nm", normal, bank), axis=1)
        order = np.argsort(partition, kind="stable")
        packets = np.split(order, np.flatnonzero(np.diff(partition[order])) + 1)
        counts = Counter(
            triangles=len(faces),
            no_back_faces=int(potential.sum()),
            vertices=len(positions),
            packets=len(packets),
        )
        for packet in packets:
            contain = normal[packet]
            highest = upper_support(
                eye, contain.min(axis=0), contain.max(axis=0), offset[packet].max()
            )
            packet_vertices = len(np.unique(faces[packet]))
            counts["stored_vertices"] += packet_vertices
            if highest >= -tolerance_m:
                counts["read_triangles"] += len(packet)
                counts["read_vertices"] += packet_vertices
                counts["reads"] += 1
            elif np.any(potential[packet]):
                raise AssertionError(
                    "packet bounds omitted a potentially facing triangle"
                )
        result.append(counts)
    return result


def replay(database, keys, observers):
    banks = [directions(count) for count in (0, 1, 2)]
    counts = [Counter() for _ in banks]
    total_native = 0
    total_stored = 0
    bake_seconds = 0.0
    with closing(
        sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True)
    ) as db:
        archive = Packages(db)
        for key in keys:
            bound = db.execute(
                "SELECT minx,miny,minz FROM assets WHERE key=?", (key,)
            ).fetchone()
            payload, stored = archive.read(key)
            walls, roofs, wall_indices, roof_indices = geometry(payload, key)
            total_native += len(payload)
            total_stored += stored
            positions = np.concatenate((walls, roofs)).view("<f4").astype(np.float64)
            if not len(positions):
                continue
            tile_min = positions.min(axis=0)
            centre = 0.5 * (positions.max(axis=0) + tile_min)
            tile_centre = np.asarray(bound) + centre - tile_min
            x, y, z = tile_centre
            latitude = np.degrees(np.arctan2(z, np.hypot(x, y)))
            longitude = np.degrees(np.arctan2(y, x))
            _, axes = local_frame(latitude, longitude)
            eyes = np.einsum("ij,jk->ik", observers - tile_centre, axes)
            eye = np.stack((eyes.min(axis=0), eyes.max(axis=0)))
            started = perf_counter()
            for vertices, indices in ((walls, wall_indices), (roofs, roof_indices)):
                for total, current in zip(
                    counts,
                    measure_surface(vertices, indices, centre, axes, eye, banks),
                ):
                    total.update(current)
            bake_seconds += perf_counter() - started
    return {
        "cells": len(keys),
        "native_bytes": total_native,
        "stored_bytes": total_stored,
        "bake_seconds": bake_seconds,
        "directions": {str(len(bank)): count for bank, count in zip(banks, counts)},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("database", type=Path)
    parser.add_argument(
        "keys", type=Path, help="one observed opaque building asset key per line"
    )
    parser.add_argument("scenario", type=Path)
    parser.add_argument("benchmark", type=Path)
    parser.add_argument(
        "--guard-m",
        type=float,
        default=2.0,
        help="expand the recorded path on all axes in scene metres",
    )
    args = parser.parse_args()
    if not np.isfinite(args.guard_m) or args.guard_m < 0:
        parser.error("a finite nonnegative path guard is required")
    keys = tuple(dict.fromkeys(args.keys.read_text().split()))
    if not keys:
        parser.error("at least one observed asset key is required")
    world = ET.parse(args.scenario).getroot().find("world")
    anchor, axes = local_frame(float(world.attrib["lat"]), float(world.attrib["lon"]))
    start, finish = camera_path(json.loads(args.benchmark.read_text()), anchor, axes)
    guard = np.asarray(list(product((-args.guard_m, args.guard_m), repeat=3))) @ axes.T
    observers = np.concatenate((start + guard, finish + guard))
    print(json.dumps(replay(args.database, keys, observers)))


if __name__ == "__main__":
    main()
