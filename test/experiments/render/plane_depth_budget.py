"""Conditional inverse-depth plane budget; no disocclusion or rendering admission.

For parallel capture/query axes and purely lateral eye translation |D|<=reuse,
the exact horizontal reprojection displacement is f*D*|1/z-1/plane_z|.
This module budgets uniform inverse-depth bins, not layers for unseen surfaces.
"""

import argparse
import hashlib
import json
import math
import tempfile
from pathlib import Path

import numpy as np
from source_billboard import projected_bounds
from source_far_assets import load_source


def budget(near, far, focal, reuse, error):
    values = (near, far, focal, reuse, error)
    if (
        not all(math.isfinite(v) for v in values)
        or not 0 < near <= far
        or focal <= 0
        or reuse < 0
        or error <= 0
    ):
        raise ValueError(
            "finite positive depth/focal/error and nonnegative lateral reuse required"
        )
    rho_low, rho_high = 1 / far, 1 / near
    span = rho_high - rho_low
    count = max(1, math.ceil(focal * reuse * span / (2 * error)))
    return {
        "planes": count,
        "inverse_low": rho_low,
        "inverse_step": span / count,
        "max_lateral_error_px": focal * reuse * span / (2 * count),
    }


def quantized_depth(depth, spec):
    rho = 1 / np.asarray(depth, float)
    if spec["inverse_step"] == 0:
        return np.full_like(rho, 1 / spec["inverse_low"])
    index = np.floor((rho - spec["inverse_low"]) / spec["inverse_step"])
    index = np.clip(index, 0, spec["planes"] - 1)
    return 1 / (spec["inverse_low"] + (index + 0.5) * spec["inverse_step"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fine", type=Path, required=True)
    parser.add_argument("--views", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reuse-m", type=float, nargs="+", default=[1.0, 6.0, 64.0])
    args = parser.parse_args()
    if not all(math.isfinite(d) and 0 <= d <= 1024 for d in args.reuse_m):
        raise ValueError("bounded nonnegative lateral comparison distances required")
    out = args.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    source = load_source(args.fine)
    views = json.loads(args.views.read_text())
    if source["metadata"] != views["sources"]:
        raise ValueError("view report belongs to different original sources")
    out.mkdir(parents=True, exist_ok=False)
    records = []
    for case in views["cases"]:
        eye, basis, focal = (
            np.array(case["eye"]),
            np.array(case["basis"]),
            views["focal_px"],
        )
        local = (source["positions"].astype(float) - eye) @ basis.T
        near, far = float(local[:, 2].min()), float(local[:, 2].max())
        box = projected_bounds(source["positions"], (eye, basis, focal, 1280, 720))
        for reuse in args.reuse_m:
            spec = budget(near, far, focal, reuse, 0.5)
            # Include horizontal image displacement at nearest z for this reuse.
            size = np.ceil(np.ptp(box, axis=0) + [2 * focal * reuse / near, 0]).astype(
                int
            )
            slots = int(size[0]) * int(size[1]) * spec["planes"]
            records.append(
                {
                    "distance_extents": case["distance_extents"],
                    "yaw": case["yaw"],
                    "translation_m": case["translation_m"],
                    "reuse_m": reuse,
                    "projected_source_span_px": (np.ptp(box, axis=0) - 4).tolist(),
                    "motion_only_seconds_at_6mps": reuse / 6,
                    "motion_only_frames_at_6mps_60fps": reuse / 6 * 60,
                    "z_range_m": [near, far],
                    "width_height": size.tolist(),
                    "dense_slots": slots,
                    "dense_unlit_field_bytes": 30 * slots,
                    **spec,
                }
            )
    report = {
        "scope": "Planning only. Parallel axes and purely horizontal lateral translation; original focal; 0.5px positional target. 1/6/64m defaults are comparison cases, not fixed engine limits. Motion-only intervals at 6m/s and 60fps exclude earlier appearance/disocclusion invalidation. No motion cube, alpha/material/shadow/PBR, coverage or world/device admission.",
        "source": source["metadata"],
        "focal_px": views["focal_px"],
        "image_width_height": [1280, 720],
        "field_bytes": {
            "source_depth_f32": 4,
            "packed_normal": 4,
            "colour_factor_rgba8": 4,
            "material_code_i16": 2,
            "uv_f32x2": 8,
            "wall_metres_f32x2": 8,
        },
        "cost_scope": "Specific 30B unlit field layout, padded rectangle and uniform inverse bins. Not a universal minimum or complete asset. Meshes, mipmaps, memberships/bindings, row offsets, allocator/cache/state, visibility-layer multiplicity and roundabout coverage excluded. True-depth fields could remove plane quantization but not unseen-surface requirements. No renderer/bake performed.",
        "views_sha256": hashlib.sha256(args.views.read_bytes()).hexdigest(),
        "code_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "cases": records,
    }
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
