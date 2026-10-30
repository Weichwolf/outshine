"""Compare the general homogeneous bound with its existing perspective special case."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
from pathlib import Path

import numpy as np
from capture_support_reference import metadata_inputs
from experiment_output import create_output
from field_capture_hull import capture_hull
from field_pose_geometry import projection_bound
from field_pose_study import DATA, ROOT, controls
from field_view_cost import measured
from field_view_projection import CapturePlane, displacement_bound
from field_view_sampling import FieldGrid


def run(args):
    create_output(args.output)
    manifest, arrays = metadata_inputs(DATA)
    lower, upper = arrays["source_least_m"].min(0), arrays["source_most_m"].max(0)
    plane, cases = controls(lower, upper, manifest["capture_camera"])
    case = next(c for c in cases if c["name"] == "64km")
    # Binary-exact square-raster focal operands make both geometric problems identical.
    matrix = np.array(
        [[1.5625, 0, 0, 0], [0, 1.5625, 0, 0], [0, 0, 0, 0.1], [0, 0, 1, 0]],
        dtype=np.float32,
    )
    grid = FieldGrid(
        plane.centre, plane.toward_eye, manifest["capture_camera"]["x_mag_m"]
    )
    hull = capture_hull(grid, complete_original_capture_domain=True)
    coarse = CapturePlane(hull["lower"], hull["upper"], grid.centre, grid.toward_eye)
    functions = {
        "existing-perspective": lambda: displacement_bound(
            plane, case["eye"], case["basis"], 800
        ),
        "homogeneous-projection": lambda: projection_bound(
            plane, case["eye"], case["basis"], matrix, (1024, 1024)
        ),
        "header-derive-on-binding": lambda: capture_hull(
            grid, complete_original_capture_domain=True
        ),
        "header-aabb-projection": lambda: projection_bound(
            coarse, case["eye"], case["basis"], matrix, (1024, 1024)
        ),
        "header-oriented-projection": lambda: projection_bound(
            coarse,
            case["eye"],
            case["basis"],
            matrix,
            (1024, 1024),
            source_hull=hull["vertices"],
        ),
    }
    expected = {k: f() for k, f in functions.items()}
    if not np.allclose(
        expected["existing-perspective"]["axis_upper_px"],
        expected["homogeneous-projection"]["axis_upper_px"],
        rtol=1e-12,
        atol=1e-12,
    ):
        raise ValueError("The compared scopes must solve the same geometric problem")
    results = {}
    for name, function in functions.items():
        value, costs = measured(function, args.output, name)
        if value != expected[name]:
            raise ValueError("Measured result differs from its correctness control")
        results[name] = {"result": value, "costs": costs}
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "python": platform.python_version(),
        "platform": platform.platform(),
        "loadavg": os.getloadavg(),
        "scope": "Normal-priority held-metadata Python calls; seven rounds per method and separate native Memray traces. Same whole group and scalar geometric requirement. General matrix/tilted plane support is distinct from speed. No cold IO, process/UMA peak, GPU arithmetic, native frame time or device budget claim.",
        "whole_hull_bytes": 48,
        "per_method_corner_visits": 8,
        "fine_vertex_visits_in_admission": 0,
        "input_npz_sha256": manifest["npz_sha256"],
        "source_sha256": {
            p: hashlib.sha256(
                (ROOT / "test/experiments/render" / p).read_bytes()
            ).hexdigest()
            for p in (
                "field_pose_geometry.py",
                "field_capture_hull.py",
                "field_pose_cost.py",
                "field_view_projection.py",
            )
        },
        "results": results,
    }
    (args.output / "costs.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
