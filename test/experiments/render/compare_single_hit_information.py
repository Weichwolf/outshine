"""Information missing from an exact first-hit capture, not a renderer admission."""

import argparse
import hashlib
import json
import subprocess
import time
from itertools import product
from pathlib import Path

import numpy as np
from compare_native_world_fields import pixels, reduce_samples
from experiment_output import create_output
from native_world_inputs import DATA, POSES, clip_from_original, load


def projected_cells(points, depth, capture_from_world, raster, dimensions):
    clip = np.c_[points / raster * [2, -2] + [-1, 1], depth, np.ones(len(depth))]
    captured = clip @ capture_from_world.T
    if not np.isfinite(captured).all() or np.any(captured[:, 3] <= 0):
        raise ValueError("Finite forward projection required for this diagnostic")
    xy = (captured[:, :2] / captured[:, 3:] * [1, -1] + 1) * dimensions / 2
    return np.floor(xy).astype(np.int64), xy


def missing_samples(source_depth, capture_depth, transfer, crop, raster):
    """Finite native samples only; perturbations are not a GPU error bound."""
    if (
        source_depth.dtype != np.float32
        or capture_depth.dtype != np.float32
        or source_depth.shape != (crop[3] - crop[1], crop[2] - crop[0])
        or not np.isfinite(source_depth).all()
        or not np.isfinite(capture_depth).all()
        or np.any((source_depth < 0) | (source_depth > 1))
        or np.any((capture_depth < 0) | (capture_depth > 1))
        or source_depth.size > 65536
        or capture_depth.size > 65536
    ):
        raise ValueError("Bounded Float32 native raw-depth planes required")
    mask = source_depth > 0
    coordinates, depth = pixels(crop)[mask.ravel()], source_depth[mask]
    dimensions = np.array(capture_depth.shape[::-1])
    cell, xy = projected_cells(coordinates, depth, transfer, raster, dimensions)
    inside = ((cell >= 0) & (cell < dimensions)).all(1)
    absent = np.zeros(len(cell), bool)
    absent[inside] = capture_depth[cell[inside, 1], cell[inside, 0]] == 0
    stable = inside.copy()
    # Explicit sensitivity probe, not an assumed bound on native arithmetic.
    for dx, dy, direction in product((-1e-6, 1e-6), (-1e-6, 1e-6), (-np.inf, np.inf)):
        perturbed = np.nextafter(depth, np.float32(direction))
        other, _ = projected_cells(
            coordinates + [dx, dy], perturbed, transfer, raster, dimensions
        )
        stable &= (other == cell).all(1)
    absent_plane, stable_plane = np.zeros_like(mask), np.zeros_like(mask)
    absent_plane[mask], stable_plane[mask] = absent, absent & stable
    fraction = xy - np.floor(xy)
    margin = np.minimum(fraction, 1 - fraction).min(1)
    rows = [
        {
            "world_sample_xy": coordinates[i].tolist(),
            "capture_xy": xy[i].tolist(),
            "capture_cell_xy": cell[i].tolist(),
            "cell_edge_margin_texels": float(margin[i]),
            "stable_under_declared_probe": bool(stable[i]),
        }
        for i in np.flatnonzero(absent)
    ]
    return (
        absent_plane,
        stable_plane,
        {
            "native_source_samples": int(mask.sum()),
            "projected_samples_outside_capture": int((~inside).sum()),
            "source_samples_projected_into_empty_capture_cells": int(absent.sum()),
            "same_empty_cell_under_declared_probe": int((absent & stable).sum()),
            "projection_evaluations": int(len(depth) * 9),
            "explicit_sensitivity_probe": "Float32 depth predecessor/successor and +/-1e-6 World sample px; no certified GPU error interval",
            "rows": rows,
        },
    )


def rectangle_samples(uv, shear, include_rear):
    """Opaque rectangles z=2 and z=1; rays x=u+shear*z, y=v, z decreasing."""
    result = np.zeros((*uv.shape[:-1], 4))
    depth = np.zeros(uv.shape[:-1])
    planes = ((1, 1.0), (2, 0.2)) if include_rear else ((2, 0.2),)
    for z, grey in planes:
        hit = (abs(uv[..., 0] + shear * z) <= 0.5) & (abs(uv[..., 1]) <= 0.5)
        result[hit] = [grey, grey, grey, 1]
        depth[hit] = z / 4
    return result, depth


def indistinguishable_pair():
    # The rear rectangle's entire orthographic footprint is covered by the front.
    y, x = np.mgrid[-1:1:128j, -1:1:128j]
    uv = np.stack((x, y), -1)
    captures = [rectangle_samples(uv, 0, rear) for rear in (False, True)]
    for a, b in zip(*captures, strict=True):
        np.testing.assert_array_equal(a, b)
    # Four samples of one novel output pixel; the front misses, the rear hits.
    novel = np.array([[[-1.1, -0.1], [-0.9, -0.1]], [[-1.1, 0.1], [-0.9, 0.1]]])
    images = [rectangle_samples(novel, 1, rear)[0] for rear in (False, True)]
    means = [image.mean((0, 1)) for image in images]
    cover_gap = float(abs(means[0][3] - means[1][3]))
    return {
        "identical_capture_rgba_and_depth": True,
        "identical_continuous_capture_reason": "The entire rear footprint [-.5,.5]^2 is behind the opaque front at z=2; adding rear z=1 changes no first hit, even at infinite capture resolution.",
        "novel_common_sample_count": 4,
        "source_coverage_gap": cover_gap,
        "worst_case_coverage_error_lower_bound": cover_gap / 2,
        "worst_case_linear_rgb_error_lower_bound": float(
            abs(means[0][0] - means[1][0]) / 2
        ),
        "scope": "Any deterministic reconstruction using only identical first-hit operands returns the same result for both scenes; triangle inequality gives the minimax bound. This analytic counterexample is not an estimate of error on the native fixture or native PBR shading. Randomization cannot guarantee both scenes either.",
    }


def audit_pose(manifest, arrays, cases, pose, output):
    crop, raster = manifest["crops_xyxy"][pose], np.array(manifest["raster"])
    capture, world = (
        clip_from_original(cases[k, pose]) for k in ("capture", "original")
    )
    source_z = arrays["original/" + pose + "/depth"]
    start = time.process_time()
    absent, stable, record = missing_samples(
        source_z,
        arrays["capture/" + pose + "/depth"],
        capture @ np.linalg.inv(world),
        crop,
        raster,
    )
    record["diagnostic_cpu_s"] = time.process_time() - start
    for outer in ("terrain", "external"):
        visible = source_z > arrays[outer + "/" + pose + "/depth"]
        uncovered = absent & visible
        record[outer] = {
            "visible_source_samples": int(visible.sum()),
            "visible_samples_in_empty_capture_cells": int(uncovered.sum()),
            "max_output_fraction_of_these_samples": float(
                reduce_samples(uncovered[:, :, None].astype(float), crop, 2).max()
            ),
        }
    record["stored_capture_payload_bytes"] = sum(
        arrays["capture/" + pose + "/" + k].nbytes for k in ("rgba", "depth")
    )
    np.savez_compressed(
        output / (pose + "-information.npz"), absent=absent, stable=stable
    )
    return record


def run(args):
    paths = [Path(__file__), Path(__file__).with_name("native_world_inputs.py")]
    if subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--untracked-files=all",
            "--",
            *map(str, paths),
        ],
        text=True,
    ).strip():
        raise ValueError("Freeze the information screen before reporting")
    manifest, arrays, cases, _ = load(args.input)
    if manifest["raster"] != [2560, 1440]:
        raise ValueError("Actual shared quarter samples for 1280x720 required")
    create_output(args.output)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_manifest_sha256": hashlib.sha256(
            (args.input / "manifest.json").read_bytes()
        ).hexdigest(),
        "input_npz_sha256": manifest["npz_sha256"],
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "analytic_counterexample": indistinguishable_pair(),
        "poses": {
            pose: audit_pose(manifest, arrays, cases, pose, args.output)
            for pose in POSES[:2]
        },
        "scope": "Information diagnostic, not a competing renderer. Native hits projecting into empty capture cells are finite observations; cells from other surfaces may still spuriously cover the novel ray. Counts are not an image-error lower bound or a certified GPU visibility/occlusion proof. No runtime admission or original replacement.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DATA / "ss2")
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
