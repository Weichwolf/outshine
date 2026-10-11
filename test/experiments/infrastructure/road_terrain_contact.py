#!/usr/bin/env python3
"""Compare node and cell contacts on an exported native terrain/road patch.

Input: two little-endian uint64 triangle counts, then float32 XYZ terrain
triangles followed by road triangles. Export only the native streets part.
"""

import json
import sys
import time
from pathlib import Path

import numpy as np


def overlaps(triangle, others):
    selected = np.ones(len(others), dtype=bool)
    for polygons in (np.broadcast_to(triangle, others.shape), others):
        edges = np.roll(polygons, -1, axis=1) - polygons
        normals = np.stack((-edges[:, :, 1], edges[:, :, 0]), axis=-1)
        for edge in range(3):
            first = np.einsum('ij,kj->ik', normals[:, edge], triangle)
            second = np.einsum('ij,ikj->ik', normals[:, edge], others)
            selected &= ((first.max(1) >= second.min(1) - 1e-10) &
                         (second.max(1) >= first.min(1) - 1e-10))
    return selected


def samples_on(roads):
    return np.asarray([(i * road[0] + j * road[1] + (24 - i - j) * road[2]) / 24
                       for road in roads for i in range(25) for j in range(25 - i)])


def measure(heights, terrain_xz, indices, samples):
    gaps = []
    origin = terrain_xz[:, 0]
    along = terrain_xz[:, 1] - origin
    across = terrain_xz[:, 2] - origin
    determinant = along[:, 0] * across[:, 1] - along[:, 1] * across[:, 0]
    valid = np.abs(determinant) > 1e-12
    determinant = np.where(valid, determinant, 1)
    for sample in samples:
        relative = sample[[0, 2]] - origin
        first = (relative[:, 0] * across[:, 1] - relative[:, 1] * across[:, 0]) / determinant
        second = (along[:, 0] * relative[:, 1] - along[:, 1] * relative[:, 0]) / determinant
        inside = valid & (first >= -1e-7) & (second >= -1e-7) & (first + second <= 1 + 1e-7)
        if inside.any():
            corners = heights[indices[inside]]
            up = ((1 - first[inside] - second[inside]) * corners[:, 0] +
                  first[inside] * corners[:, 1] + second[inside] * corners[:, 2])
            gaps.append(float(up.max() - sample[1]))
    return dict(samples=len(gaps), outside_patch=len(samples) - len(gaps),
                above_5cm=sum(gap > .05 for gap in gaps), max_above_m=max(gaps))


def compare(terrain, roads):
    terrain_xz = terrain[:, :, [0, 2]]
    positions, indices = np.unique(terrain_xz.reshape(-1, 2), axis=0, return_inverse=True)
    indices = indices.reshape(-1, 3)
    initial = np.full(len(positions), np.inf)
    np.minimum.at(initial, indices.ravel(), terrain[:, :, 1].ravel())
    planes = np.stack([np.linalg.solve(np.column_stack((road[:, 0], road[:, 2], np.ones(3))),
                                      road[:, 1]) for road in roads])
    samples = samples_on(roads)
    result = dict(terrain_triangles=len(terrain), road_triangles=len(roads),
                  baseline=measure(initial, terrain_xz, indices, samples), candidates={})
    for mode in ('node_inside', 'triangle_overlap', 'cell_radius'):
        began = time.perf_counter()
        heights = initial.copy()
        pairs = 0
        for road, plane in zip(roads[:, :, [0, 2]], planes):
            if mode != 'triangle_overlap':
                edges = np.roll(road, -1, axis=0) - road
                cross = (edges[:, 0, None] * (positions[:, 1] - road[:, 1, None]) -
                         edges[:, 1, None] * (positions[:, 0] - road[:, 0, None]))
                inside = (cross >= -1e-7).all(0) | (cross <= 1e-7).all(0)
                if mode == 'cell_radius':
                    intersecting = ((terrain_xz.max(1) >= road.min(0)).all(1) &
                                    (terrain_xz.min(1) <= road.max(0)).all(1))
                    cells = terrain_xz[intersecting]
                    radius = np.linalg.norm(np.roll(cells, -1, axis=1) - cells, axis=2).max()
                    for begin, edge in zip(road, edges):
                        along = np.clip(np.sum((positions - begin) * edge, axis=1) / np.sum(edge * edge), 0, 1)
                        delta = positions - begin - along[:, None] * edge
                        inside |= np.sum(delta * delta, axis=1) <= radius * radius
                chosen = np.flatnonzero(inside)
            else:
                intersecting = overlaps(road, terrain_xz)
                pairs += int(intersecting.sum())
                chosen = np.unique(indices[intersecting])
            floor = positions[chosen, 0] * plane[0] + positions[chosen, 1] * plane[1] + plane[2] - .05
            heights[chosen] = np.minimum(heights[chosen], floor)
        elapsed = (time.perf_counter() - began) * 1000
        result['candidates'][mode] = dict(
            **measure(heights, terrain_xz, indices, samples),
            changed_nodes=int((heights < initial - 1e-6).sum()),
            max_cut_m=float((initial - heights).max()), python_apply_ms=elapsed,
            overlap_pairs=pairs)
    for mode in ('triangle_overlap', 'cell_radius'):
        assert result['candidates'][mode]['max_above_m'] <= -.05 + 1e-6
    return result


def main(path):
    raw = Path(path).read_bytes()
    terrain_count, road_count = map(int, np.frombuffer(raw, dtype='<u8', count=2))
    assert terrain_count > 0 and road_count > 0
    assert len(raw) == 16 + (terrain_count + road_count) * 9 * 4
    values = np.frombuffer(raw, dtype='<f4', offset=16).astype('float64')
    assert np.isfinite(values).all()
    terrain = values[:terrain_count * 9].reshape(-1, 3, 3)
    roads = values[terrain_count * 9:].reshape(-1, 3, 3)
    print(json.dumps(compare(terrain, roads), indent=2))


if __name__ == '__main__':
    main(sys.argv[1])
