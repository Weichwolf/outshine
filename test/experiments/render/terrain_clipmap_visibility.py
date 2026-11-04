"""Conservative clipmap block selection; reference geometry, not a native work plan."""

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import sys

import numpy as np

from terrain_clipmap import layout, positions
from terrain_clipmap_cache import HeightClipmap
from terrain_clipmap_source import NativeTerrain
from terrain_clipmap_view import camera


@dataclass(frozen=True)
class Blocks:
    lower: np.ndarray
    upper: np.ndarray
    owners: np.ndarray
    triangles: np.ndarray


def partition(mesh, vertices, block_cells):
    if not isinstance(block_cells, int) or block_cells < 1:
        raise ValueError("a positive integer block size is required")
    points = np.asarray(vertices, dtype=np.float64)[mesh.triangles]
    levels = mesh.levels[mesh.triangles].max(axis=1)
    width = mesh.cell_m * block_cells * np.exp2(levels.astype(np.float64))
    tiles = np.floor(points[:, :, :2].mean(axis=1) / width[:, None]).astype(np.int64)
    keys = np.column_stack((levels, tiles))
    unique, owners = np.unique(keys, axis=0, return_inverse=True)
    lower = np.full((len(unique), 3), np.inf)
    upper = np.full((len(unique), 3), -np.inf)
    np.minimum.at(lower, owners, points.min(axis=1))
    np.maximum.at(upper, owners, points.max(axis=1))
    return Blocks(lower, upper, owners, np.bincount(owners))


def visible(blocks, view, near=0.1):
    eye, basis, focal, width, height = view
    eye, basis = np.asarray(eye), np.asarray(basis)
    if (eye.shape != (3,) or basis.shape != (3, 3) or
            not np.all(np.isfinite(eye)) or not np.all(np.isfinite(basis)) or
            not np.all(np.isfinite([focal, width, height, near])) or
            min(focal, width, height, near) <= 0 or np.linalg.det(basis) == 0):
        raise ValueError("a finite nondegenerate perspective view is required")
    right, up, forward = basis
    normals = np.stack((forward * width / (2 * focal) + right,
                        forward * width / (2 * focal) - right,
                        forward * height / (2 * focal) + up,
                        forward * height / (2 * focal) - up, forward))
    offset = -normals @ eye
    offset[-1] -= near
    positive = np.where(normals[None, :, :] >= 0,
                        blocks.upper[:, None, :], blocks.lower[:, None, :])
    terms = positive * normals[None, :, :]
    maximum = terms.sum(axis=2) + offset
    tolerance = 64 * np.finfo(np.float64).eps * (
        np.abs(terms).sum(axis=2) + np.abs(offset) + 1)
    known = np.all(np.isfinite(blocks.lower) & np.isfinite(blocks.upper), axis=1)
    return ~known | np.all(maximum >= -tolerance, axis=1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--key", required=True)
    parser.add_argument("--input-byte-budget", type=int, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--levels", type=int, default=9)
    parser.add_argument("--half-cells", type=int, default=128)
    parser.add_argument("--cell-m", type=float, default=1)
    parser.add_argument("--block-cells", type=int, nargs="+", default=[8, 16, 32])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = NativeTerrain.from_cache(args.cache, args.key, args.input_byte_budget)
    view = camera(args.scenario, source, 1280, 720, 0)
    mesh = layout(view[0][:2], args.levels, args.half_cells, args.cell_m)
    heights = HeightClipmap(source, args.levels, args.half_cells, args.cell_m)
    heights.move(view[0][:2])
    vertices = positions(mesh, heights)
    variants = []
    for cells in args.block_cells:
        blocks = partition(mesh, vertices, cells)
        kept = [int(blocks.triangles[visible(
            blocks, camera(args.scenario, source, 1280, 720, yaw))].sum())
                for yaw in range(0, 360, 6)]
        variants.append(dict(block_cells=cells, blocks=len(blocks.lower), retained=kept,
                             retained_fraction=float(np.mean(kept) / len(mesh.triangles))))
    report = dict(command=sys.argv, source=source.provenance,
                  scenario_sha256=hashlib.sha256(args.scenario.read_bytes()).hexdigest(),
                  levels=args.levels, half_cells=args.half_cells, cell_m=args.cell_m,
                  triangles=len(mesh.triangles), bounds=mesh.outer_bounds.tolist(), variants=variants,
                  scope="Reference geometry and 60 yaw samples. Bounds cover these rendered vertices; "
                        "native implementation must bound parent morph support and height updates. "
                        "No occlusion, shadow, native timing or full-world/device qualification.")
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
