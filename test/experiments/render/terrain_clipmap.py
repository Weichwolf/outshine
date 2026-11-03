"""Camera-centred clipmap topology and geometric morphing reference for #515."""

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import time

import numpy as np


@dataclass(frozen=True)
class Clipmap:
    xy: np.ndarray
    triangles: np.ndarray
    levels: np.ndarray
    camera: np.ndarray
    cell_m: float
    half_cells: int
    outer_bounds: np.ndarray


def triangle_count(levels, half_cells):
    side = 2 * half_cells
    return 2 * side**2 + (levels - 1) * (3 * side**2 // 2 + 2 * side)


def ring(camera, level, half_cells, inner):
    step = 1 << level
    centre = np.floor(camera / (2 * step)).astype(np.int64) * (2 * step)
    low, high = centre - half_cells * step, centre + half_cells * step
    y, x = np.indices((2 * half_cells, 2 * half_cells), dtype=np.int64)
    cells = np.column_stack((x.ravel(), y.ravel())) * step + low
    if inner is not None:
        inside = np.all((cells >= inner[0]) & (cells < inner[1]), axis=1)
        cells = cells[~inside]
    corners = cells[:, None, :] + np.array(
        [[0, 0], [step, 0], [step, step], [0, step]], dtype=np.int64
    )
    edges = np.full(len(cells), -1, dtype=np.int8)
    if inner is not None:
        a, b = inner
        horizontal = (cells[:, 0] >= a[0]) & (cells[:, 0] < b[0])
        vertical = (cells[:, 1] >= a[1]) & (cells[:, 1] < b[1])
        edges[(cells[:, 1] == b[1]) & horizontal] = 0
        edges[(cells[:, 0] + step == a[0]) & vertical] = 1
        edges[(cells[:, 1] + step == a[1]) & horizontal] = 2
        edges[(cells[:, 0] == b[0]) & vertical] = 3
    regular = corners[edges < 0]
    faces = [regular[:, [0, 1, 2]], regular[:, [0, 2, 3]]]
    for edge in range(4):
        selected = corners[edges == edge]
        a, b, c, d = (selected[:, (edge + i) % 4] for i in range(4))
        middle = (a + b) // 2
        faces.extend(
            np.stack(points, axis=1)
            for points in [(d, a, middle), (d, middle, b), (d, b, c)]
        )
    return np.concatenate(faces), np.stack((low, high))


def layout(camera=(0.0, 0.0), levels=13, half_cells=64, cell_m=1.0):
    camera = np.asarray(camera, dtype=np.float64)
    if camera.shape != (2,) or not np.all(np.isfinite(camera)):
        raise ValueError("a finite two-dimensional camera is required")
    if levels < 1 or half_cells < 8 or half_cells % 2 or cell_m <= 0 or not np.isfinite(cell_m):
        raise ValueError("positive levels/cell size and an even half-grid of at least eight cells required")
    extent = half_cells * (1 << (levels - 1))
    units = camera / cell_m
    if np.max(np.abs(units)) + 2 * extent >= np.iinfo(np.int64).max:
        raise ValueError("clipmap coordinates exceed the integer topology domain")
    if 3 * triangle_count(levels, half_cells) > np.iinfo(np.uint32).max:
        raise ValueError("clipmap index domain exceeds uint32")
    faces, owners = [], []
    inner = None
    for level in range(levels):
        indices, inner = ring(units, level, half_cells, inner)
        faces.append(indices)
        owners.append(np.full(len(indices), level, dtype=np.uint8))
    coordinates = np.concatenate(faces)
    face_levels = np.concatenate(owners)
    points, inverse = np.unique(coordinates.reshape(-1, 2), axis=0, return_inverse=True)
    finest = np.full(len(points), levels, dtype=np.uint8)
    np.minimum.at(finest, inverse, np.repeat(face_levels, 3))
    return Clipmap(
        points.astype(np.float64) * cell_m,
        inverse.reshape(-1, 3).astype(np.uint32),
        finest,
        camera,
        cell_m,
        half_cells,
        inner.astype(np.float64) * cell_m,
    )


def coarse_height(points, steps, sample):
    low = np.floor(points / steps[:, None]) * steps[:, None]
    uv = (points - low) / steps[:, None]
    a = sample(low)
    b = sample(low + np.column_stack((steps, np.zeros_like(steps))))
    c = sample(low + steps[:, None])
    d = sample(low + np.column_stack((np.zeros_like(steps), steps)))
    u, v = uv.T
    return np.where(u >= v, (1 - u) * a + (u - v) * b + v * c,
                    (1 - v) * a + (v - u) * d + u * c)


def positions(mesh, sample):
    heights = np.asarray(sample(mesh.xy), dtype=np.float64)
    if heights.shape != (len(mesh.xy),) or not np.all(np.isfinite(heights)):
        raise ValueError("the height sampler must cover every requested vertex")
    step = mesh.cell_m * np.exp2(mesh.levels.astype(np.float64))
    end = (mesh.half_cells - 2) * step
    finer_ring_reach = (mesh.half_cells // 2 + 1) * step
    width = np.minimum(8 * step, end - finer_ring_reach)
    distance = np.max(np.abs(mesh.xy - mesh.camera), axis=1)
    weight = np.clip((distance - (end - width)) / width, 0.0, 1.0)
    weight[mesh.levels == mesh.levels.max()] = 0.0
    selected = weight > 0
    target = coarse_height(mesh.xy[selected], 2 * step[selected], sample)
    if not np.all(np.isfinite(target)):
        raise ValueError("the height sampler must also cover morph support")
    heights[selected] += weight[selected] * (target - heights[selected])
    return np.column_stack((mesh.xy, heights))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--camera", type=float, nargs=2, default=(0.0, 0.0))
    parser.add_argument("--levels", type=int, default=13)
    parser.add_argument("--half-cells", type=int, default=64)
    parser.add_argument("--cell-m", type=float, default=1.0)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    began = time.perf_counter()
    mesh = layout(args.camera, args.levels, args.half_cells, args.cell_m)
    args.output.mkdir(parents=True, exist_ok=False)
    np.savez_compressed(args.output / "topology.npz", xy=mesh.xy, triangles=mesh.triangles,
                        levels=mesh.levels)
    report = dict(vertices=len(mesh.xy), triangles=len(mesh.triangles),
                  bounds_m=mesh.outer_bounds.tolist(), build_and_export_ms=1000 * (time.perf_counter() - began),
                  scope="Python topology reference; no native timing or terrain-image acceptance")
    (args.output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
