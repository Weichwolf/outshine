"""World-frame metadata mapping: exact geometry and one bounded cost comparison."""

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
from field_pose_geometry import geometric_candidate, operands, projection_matrix_bound
from field_pose_study import DATA, controls
from field_view_cost import measured
from field_view_projection import CapturePlane, dot, point, subtract
from world_field_pose import clip_from_field, projection_bound, rigid_frame

ROOT = Path(__file__).resolve().parents[3]


def full_homogeneous_clip(frame, eye, basis, P, raster):
    """Correct but cancellation-prone alternative; do not form relative T-eye first."""
    frame = rigid_frame(frame)
    eye, basis, P, raster = operands(eye, basis, P, raster)
    A = tuple(
        (
            *tuple(map(point, row)),
            subtract(point(0.0), dot(tuple(map(point, row)), tuple(map(point, eye)))),
        )
        for row in basis
    )
    A += ((point(0.0), point(0.0), point(0.0), point(1.0)),)

    def multiply(a, b):
        return tuple(
            tuple(dot(row, tuple(b[k][j] for k in range(4))) for j in range(4))
            for row in a
        )

    W = tuple(tuple(map(point, row)) for row in frame)
    return multiply(multiply(P, A), W), raster


def run(args):
    files = [
        Path(__file__).with_name(name)
        for name in (
            "world_field_pose_study.py",
            "world_field_pose.py",
            "field_pose_geometry.py",
            "field_view_projection.py",
            "field_pose_study.py",
            "capture_support_reference.py",
            "field_view_cost.py",
        )
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, files)], text=True
    ):
        raise ValueError("Freeze measured sources first")
    create_output(args.output)
    manifest, arrays = metadata_inputs(DATA)
    lower, upper = arrays["source_least_m"].min(0), arrays["source_most_m"].max(0)
    plane, cases = controls(lower, upper, manifest["capture_camera"])
    archive = ROOT / "test/outshine/data/render/building-full-view"
    request = json.loads((archive / "source-plane-request.json").read_text())
    context = json.loads((archive / "context.json").read_text())
    frame = np.eye(4)
    frame[:3, :3] = np.asarray(
        [request["east"], request["up"], -np.asarray(request["north"])]
    ).T
    frame[:3, 3] = np.asarray(context["scene_frame"]["origin_ecef"]) + request["origin"]
    R = frame[:3, :3]
    rows = []
    for case in cases:
        eye = frame[:3, 3] + R @ case["eye"]
        basis = np.asarray(case["basis"]) @ R.T
        result = projection_bound(
            plane, frame, eye, basis, case["projection"], case["raster"]
        )
        full, raster = full_homogeneous_clip(
            frame, eye, basis, case["projection"], case["raster"]
        )
        other = projection_matrix_bound(plane, full, raster)
        clip, _ = clip_from_field(frame, eye, basis, case["projection"], case["raster"])
        rows.append(
            {
                "case": case["name"],
                "relative": result,
                "full_homogeneous": other,
                "relative_max_coefficient_width": max(
                    v[1] - v[0] for row in clip for v in row
                ),
                "full_max_coefficient_width": max(
                    v[1] - v[0] for row in full for v in row
                ),
                "relative_candidate": geometric_candidate(result, 0.25),
                "full_candidate": geometric_candidate(other, 0.25),
            }
        )
    selected = next(c for c in cases if c["name"] == "64km")
    eye = frame[:3, 3] + R @ selected["eye"]
    basis = np.asarray(selected["basis"]) @ R.T

    def relative():
        return projection_bound(
            plane, frame, eye, basis, selected["projection"], selected["raster"]
        )

    def full():
        matrix, raster = full_homogeneous_clip(
            frame, eye, basis, selected["projection"], selected["raster"]
        )
        return projection_matrix_bound(plane, matrix, raster)

    costs = {}
    for name, call in (
        ("relative-translation-first", relative),
        ("full-homogeneous", full),
    ):
        result, cost = measured(call, args.output, name)
        costs[name] = {"result": result, "cost": cost}
    counter_plane = CapturePlane((-5, -5, -0.2), (5, 5, 0.2), (0, 0, 0), (0, 0, 1))
    counter_frame = np.eye(4)
    counter_frame[:3, 3] = [0, 0, 6378137.125]
    counter_eye = counter_frame[:3, 3] + [1.5, 0, 64.375]
    counter_basis = np.diag([1.0, 1.0, -1.0])
    P = np.asarray(
        [[700 / 640, 0, 0, 0], [0, 700 / 360, 0, 0], [0, 0, 0, 0.1], [0, 0, 1, 0]],
        "<f4",
    )
    correct = projection_bound(
        counter_plane, counter_frame, counter_eye, counter_basis, P, (1280, 720)
    )
    rounded = projection_bound(
        counter_plane,
        counter_frame.astype("<f4").astype(float),
        counter_eye.astype("<f4").astype(float),
        counter_basis,
        P,
        (1280, 720),
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files
        },
        "platform": platform.platform(),
        "load_average": list(os.getloadavg()),
        "numpy": np.__version__,
        "metadata_sources": len(arrays["source_ids"]),
        "source_metadata_array_bytes": sum(a.nbytes for a in arrays.values()),
        "frame": frame.tolist(),
        "rows": rows,
        "costs": costs,
        "float32_ecef_counter": {"correct": correct, "rounded_wrong_problem": rounded},
        "scope": "Geometric diagnostic placements using original archived axes/source bounds. NOT an authoritative normal WorldFromField or Worldrevision; existing runtime-null remains null. Metadata receipt/NPZ checksum preparation excluded from query costs and reads the archive; no avoided physical IO/UMA/device/frame claim. Relative clip numeric operands352B plus96B plane/8B raster; interval clip output256B, Python objects additional.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "report": str(args.output / "report.json"),
                "source_bounds": report["metadata_sources"],
                "cases": len(rows),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
