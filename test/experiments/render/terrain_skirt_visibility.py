"""Compare source terrain skirts with height-independent side-plane rejection."""

import argparse
import csv
import hashlib
import json
import tempfile
import time
from pathlib import Path

import numpy as np
from current_visibility import box_bounds
from terrain_grid_lod import page_frame, page_geometry, page_steps, raster_view
from terrain_occluders import skirts


def visible_sides(corners, up, eye, guard=0.0):
    corners, up, eye = (np.asarray(value, dtype=float) for value in (corners, up, eye))
    if corners.shape != (4, 3) or up.shape != (3,) or eye.shape != (3,):
        raise ValueError("four ordered corners, extrusion direction and eye required")
    if not np.isfinite(guard) or guard < 0:
        raise ValueError("the uncertainty distance must be finite and nonnegative")
    if not all(np.isfinite(value).all() for value in (corners, up, eye)):
        return np.ones(4, dtype=bool)
    with np.errstate(over="ignore", invalid="ignore"):
        normals = np.cross(np.roll(corners, -1, axis=0) - corners, -up)
        lengths = np.linalg.norm(normals, axis=1)
        score = np.einsum("ij,ij->i", normals, eye - corners)
        return (
            (~np.isfinite(score))
            | (~np.isfinite(lengths))
            | (lengths == 0)
            | (score >= -guard * lengths)
        )


def compare(pages, scenario):
    began = time.perf_counter()
    with (pages / "pages.csv").open() as source:
        rows = list(csv.DictReader(source))
    views = [
        raster_view(scenario, rows, pages, 1280, 720, yaw) for yaw in (0, 90, 180, 270)
    ]
    eye = views[0][0]
    fingerprint = hashlib.sha256((pages / "pages.csv").read_bytes())
    totals = np.zeros((4, 4), dtype=np.int64)
    groups = [set() for _ in views]
    original_groups = [set() for _ in views]
    masks = np.zeros(16, dtype=np.int64)
    checked, unsafe = 0, 0
    for row in rows:
        fractions, local, _, _, page_hash = page_geometry(row, pages)
        if len(fractions) != 33 or fractions[0] != 0 or fractions[-1] != 1:
            raise ValueError(
                "the comparison requires the complete native 33-node boundary"
            )
        if not np.all(np.diff(fractions) > 0):
            raise ValueError(
                "unordered source fractions cannot use a side-plane decision"
            )
        fingerprint.update(page_hash)
        centre, axes, corners = page_frame(row)
        step = max(*page_steps(corners, 33), 0.001)
        points, faces, _ = skirts(local, 16 * step)
        faces = faces.reshape(4, 64, 3)[[0, 3, 1, 2]].reshape(-1, 3)
        corners = np.column_stack((corners[[0, 1, 3, 2]], np.zeros(4)))
        eye_local = (eye - centre) @ axes.T
        guard = (
            64
            * np.finfo(np.float32).eps
            * (1 + np.linalg.norm(eye_local) + np.abs(local).max() + 16 * step)
        )
        keep = visible_sides(corners, np.array([0.0, 0.0, 1.0]), eye_local, guard)
        mask = int(keep @ (1 << np.arange(4)))
        masks[mask] += 1
        vertices = points @ axes + centre
        tri = vertices[faces]
        normals = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
        front = np.einsum("ij,ij->i", normals, eye - tri[:, 0]) > 0
        unsafe += int(np.count_nonzero(front & ~np.repeat(keep, 64)))
        checked += len(front)
        for n, view in enumerate(views):
            if box_bounds(
                (vertices.min(axis=0)[None], vertices.max(axis=0)[None]), view
            )[4][0]:
                totals[n] += [1, 256, int(keep.sum()) * 64, int(front.sum())]
                original_groups[n].add(row["virtual"])
                if mask:
                    groups[n].add((row["virtual"], mask))
    if unsafe:
        raise AssertionError(f"{unsafe} source front faces were discarded")
    return {
        "pages": len(rows),
        "input_sha256": fingerprint.hexdigest(),
        "scenario_sha256": hashlib.sha256(scenario.read_bytes()).hexdigest(),
        "checked_source_skirt_triangles": checked,
        "discarded_front_facing_triangles": unsafe,
        "side_masks_all_pages": masks.tolist(),
        "views": [
            {
                "yaw": yaw,
                **dict(
                    zip(
                        (
                            "pages",
                            "baseline_references",
                            "candidate_references",
                            "front_facing_source_triangles",
                        ),
                        map(int, counts),
                        strict=True,
                    )
                ),
                "original_batches": len(original_groups[n]),
                "candidate_batches": len(groups[n]),
            }
            for n, (yaw, counts) in enumerate(
                zip((0, 90, 180, 270), totals, strict=True)
            )
        ],
        "seconds": time.perf_counter() - began,
        "scope": "CPU source geometry only. Guard heuristic, not native rounding proof. "
        "Height pages read for independent verification, not the plane decision.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pages", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("output belongs in the system temp directory")
    if output.is_relative_to(Path(__file__).resolve().parents[3]):
        raise ValueError("private outputs must stay outside the checkout")
    output.parent.mkdir(parents=True, exist_ok=True)
    result = compare(args.pages, args.scenario)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
