"""Real recipe frames and deliberately retained shading counterexamples (#1531)."""

import argparse
import hashlib
import json
import lzma
import os
import platform
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from road_surface_frames import attributes, triangle_normals, unit
from road_surface_lod import prepare
from road_surface_model import Curve


def angular_error(first, second):
    # atan2 avoids the large relative acos error near coincident Float32 normals.
    first, second = unit(first.astype(float)), unit(second.astype(float))
    return np.degrees(
        np.arctan2(
            np.linalg.norm(np.cross(first, second), axis=-1),
            np.sum(first * second, axis=-1),
        )
    )


def sample_normals(curve, plan, vertex_normals, stations, lateral):
    """Interpolate over the actual indexed strip diagonal in its metric chart."""
    grid = plan["stations_m"]
    i = np.clip(np.searchsorted(grid, stations, side="right") - 1, 0, len(grid) - 2)
    u = np.clip(np.searchsorted(curve.across, lateral, side="right") - 1, 0, 2)
    if curve.shoulder == 0:
        u[:] = 1
    a = ((stations - grid[i]) / (grid[i + 1] - grid[i]))[:, None, None]
    b = ((lateral - curve.across[u]) / (curve.across[u + 1] - curve.across[u]))[
        None, :, None
    ]
    n00, n01 = vertex_normals[i[:, None], u], vertex_normals[i[:, None], u + 1]
    n10, n11 = vertex_normals[i[:, None] + 1, u], vertex_normals[i[:, None] + 1, u + 1]
    first = (1 - a - b) * n00 + a * n10 + b * n01
    second = (1 - b) * n10 + (1 - a) * n01 + (a + b - 1) * n11
    return unit(np.where(a + b <= 1, first, second))


def witness(curve, plan, frames, control):
    stations = np.linspace(curve.stations[0], curve.stations[-1], 257)
    lateral = np.linspace(-curve.half_width, curve.half_width, 65)
    exact = attributes(curve, stations, lateral_m=lateral)["normals_enh"]
    analytic = sample_normals(curve, plan, frames["normals_enh"], stations, lateral)
    triangles = sample_normals(curve, plan, control, stations, lateral)
    return {
        "sample_stations_m": stations,
        "sample_lateral_m": lateral,
        "reference_normals": exact,
        "interpolated_recipe_normals": analytic,
        "interpolated_triangle_normals": triangles,
        "recipe_error_degrees": angular_error(analytic, exact),
        "triangle_error_degrees": angular_error(triangles, exact),
        "surface_positions_enh_m": curve.at(stations),
        "mesh_positions_enh_m": plan["positions_enh_m"],
        "mesh_indices": plan["indices"],
    }


def check(row, arrays):
    # Emitted positions/normals are excluded from construction input.
    source = {key: row[key] for key in ("range_m", "section_m", "seams", "intervals")}
    curve = Curve(source)
    plan = prepare(curve, 0.25)
    before = [plan[key].copy() for key in ("stations_m", "positions_enh_m", "indices")]
    start = time.process_time_ns()
    frames = attributes(curve, plan["stations_m"])
    frame_ns = time.process_time_ns() - start
    start = time.process_time_ns()
    control, used = triangle_normals(plan)
    triangle_ns = time.process_time_ns() - start
    for name, original in zip(
        ("stations_m", "positions_enh_m", "indices"), before, strict=True
    ):
        np.testing.assert_array_equal(plan[name], original)
    left = attributes(curve, curve.stations, side="left")["normals_enh"]
    right = attributes(curve, curve.stations, side="right")["normals_enh"]
    error = angular_error(frames["normals_enh"][used], control[used])
    native_stations = np.array(row["stations_m"])
    native = np.array(row["upper_normals"]).reshape(-1, 4, 3)
    native = native[:, :, [0, 2, 1]] * [1, -1, 1]
    original_error = angular_error(
        attributes(curve, native_stations)["normals_enh"], native
    )
    ordinal = row["ordinal"]
    for name, data in frames.items():
        arrays[f"curve_{ordinal}_{name}"] = data
    # Fixed actual long/tiny records preserve a useful case and an adverse one.
    sampled = {}
    if ordinal in (8329, 32370):
        raw = witness(curve, plan, frames, control)
        for name, data in raw.items():
            arrays[f"witness_{ordinal}_{name}"] = data
        sampled = {
            name: float(raw[name].max())
            for name in ("recipe_error_degrees", "triangle_error_degrees")
        }
    return {
        "ordinal": ordinal,
        "stations": len(plan["stations_m"]),
        "length_m": float(np.ptp(curve.stations)),
        "frame_cpu_ns": frame_ns,
        "triangle_normal_cpu_ns": triangle_ns,
        "frame_output_bytes": sum(a.nbytes for a in frames.values()),
        "maximum_vertex_triangle_error_degrees": float(error.max()),
        "maximum_original_normal_model_error_degrees": float(original_error.max()),
        "maximum_source_seam_jump_degrees": float(angular_error(left, right).max()),
        "sampled_errors": sampled,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    folder = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/transport/road_curve_requests"
    )
    manifest = json.loads((folder / "manifest.json").read_text())
    archive = folder / manifest["archive"]
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    assert digest == manifest["archive_sha256"]
    rows, arrays = [], {}
    with lzma.open(archive, "rt") as stream:
        for line in stream:
            rows.append(check(json.loads(line), arrays))
    assert len(rows) == manifest["records"]
    peak_before_metadata = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True)
        ),
        "platform": platform.platform(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "source_sha256": digest,
        "implementation_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                "compare_road_frames.py",
                "road_surface_frames.py",
                "road_surface_model.py",
                "road_surface_lod.py",
                "test_road_surface_frames.py",
                "plot_road_frames.py",
            )
        },
        "frame_output_bytes": sum(r["frame_output_bytes"] for r in rows),
        "frame_cpu_ns": sum(r["frame_cpu_ns"] for r in rows),
        "triangle_normal_cpu_ns": sum(r["triangle_normal_cpu_ns"] for r in rows),
        "queried_frame_vertices": sum(r["stations"] * 4 for r in rows),
        "minimum_final_frame_allocations": 3 * len(rows),
        "loop_peak_rss_bytes_before_metadata": peak_before_metadata,
        "whole_process_peak_rss_bytes": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Recipe vertex frames versus area-weighted triangle normals. Full frame timing also includes tangents and metric UV; unequal output, no equal-quality speed claim. No angular interpolation bound, native material, port, World or device admission. Peaks include source decode and saved diagnostics.",
        "rows": rows,
    }
    np.savez_compressed(args.output / "frames.npz", **arrays)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Checked {len(rows)} recipes; frame bytes {report['frame_output_bytes']}")
    for row in rows:
        if row["sampled_errors"]:
            print(row["ordinal"], row["sampled_errors"])


if __name__ == "__main__":
    main()
