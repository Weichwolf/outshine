"""One complete captured road recipe and one declared normal refinement method.

Separate invocations give separate process ownership. Timings are development
observations unless an independent repeated comparison is performed. Normal and
Lambert diagnostics do not qualify physical profiles, PBR, GPU or World use.
"""

import argparse
import hashlib
import json
import lzma
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from experiment_output import create_output
from road_normal_lod import NormalBudget, prepare, sample
from road_surface_frames import attributes
from road_surface_lod import prepare as position_only
from road_surface_model import Curve

ROOT = Path(__file__).resolve().parents[3]
DATA = ROOT / "test/outshine/data/transport/road_curve_requests"


def read_row(ordinal):
    manifest = json.loads((DATA / "manifest.json").read_text())
    path = DATA / manifest["archive"]
    if (
        manifest["schema"] != 2
        or path.stat().st_size != manifest["archive_bytes"]
        or hashlib.sha256(path.read_bytes()).hexdigest() != manifest["archive_sha256"]
    ):
        raise ValueError("Exact captured recipe archive required")
    selected = []
    digest = hashlib.sha256()
    count = size = 0
    with lzma.open(path, "rb") as stream:
        for raw in stream:
            row = json.loads(raw)
            digest.update(raw)
            count += 1
            size += len(raw)
            if row["ordinal"] == ordinal:
                selected.append(row)
    if (
        count != manifest["records"]
        or size != manifest["uncompressed_bytes"]
        or len(selected) != 1
    ):
        raise ValueError("Complete unique original recipe and raw archive required")
    # source_jsonl_sha256 identifies the full capture, not this selected subset.
    return selected[0], manifest | {
        "selected_raw_jsonl_sha256_observed": digest.hexdigest()
    }


def generate(row, method, error_m, angle_rad, max_queries, max_vertices):
    curve = Curve(row)
    if method == "position-only":
        position = position_only(curve, error_m)
        stations = position["stations_m"]
        lateral, selected = np.unique(curve.across, return_index=True)
        fields = attributes(curve, stations, lateral_m=lateral)
        count = len(lateral)
        lower = np.arange(len(stations) - 1, dtype=np.uint32)[:, None] * count
        lower = lower + np.arange(count - 1, dtype=np.uint32)
        plan = {
            "stations_m": stations,
            "lateral_m": lateral,
            "positions_enh_m": position["positions_enh_m"][:, selected].reshape(-1, 3),
            "normals_enh": fields["normals_enh"].reshape(-1, 3),
            "station_lateral_m": fields["station_lateral_m"].reshape(-1, 2),
            "indices": np.stack(
                (
                    lower,
                    lower + count,
                    lower + 1,
                    lower + 1,
                    lower + count,
                    lower + count + 1,
                ),
                axis=-1,
            ).reshape(-1, 3),
            "cell_vertex_rows": np.column_stack(
                (np.arange(len(stations) - 1), np.arange(1, len(stations)))
            ).astype(np.uint32)
            * count,
        }
        plan.update(
            maximum_stored_position_bound_m=position["maximum_stored_bound_m"],
            whole_normal_queries=0,
            refinement_rounds=0,
            normal_limit_rad=None,
            scope="Existing position-only geometry with its sampled Float32 analytic vertex normals. May straddle source seams; no continuous angle admission. Duplicate unreferenced zero-width shoulder rows canonicalized, referenced positions unchanged.",
        )
    else:
        plan = prepare(
            curve,
            error_m,
            angle_rad,
            uniform=method == "uniform",
            max_queries=max_queries,
            max_vertices=max_vertices,
        )
    return plan, curve


def diagnostic(plan, curve):
    stations = np.unique(
        np.r_[np.linspace(*curve.stations[[0, -1]], 257), curve.stations]
    )
    lateral = np.linspace(-curve.width, curve.width, 129)
    expected = attributes(curve, stations, lateral_m=lateral)["normals_enh"]
    actual = sample(plan, stations, lateral)
    angle = np.arctan2(
        np.linalg.norm(np.cross(actual, expected), axis=-1),
        np.sum(actual * expected, axis=-1),
    )
    light = np.array([-0.35, -0.4, 1.0])
    light /= np.linalg.norm(light)
    images = {
        "diagnostic_stations_m": stations,
        "diagnostic_lateral_m": lateral,
        "observed_normal_error_rad": angle,
        "analytic_normals_enh": expected,
        "interpolated_normals_enh": actual,
        "analytic_lambert": np.maximum(0, expected @ light),
        "interpolated_lambert": np.maximum(0, actual @ light),
    }
    return images, {
        "sample_stations": len(stations),
        "sample_lateral": len(lateral),
        "observed_maximum_normal_error_rad": float(angle.max()),
        "observed_lambert_maximum_absolute_error": float(
            abs(images["analytic_lambert"] - images["interpolated_lambert"]).max()
        ),
        "scope": "Finite parameter-space diagnostic, including source seams with right-sided values. The continuous bound comes from generation, not these samples. This is not native perspective interpolation or PBR.",
    }


def run(args):
    files = [
        Path(__file__).with_name(name)
        for name in (
            "road_normal_study.py",
            "road_normal_lod.py",
            "road_normal_cone.py",
            "road_normal_math.py",
            "road_normal_hulls.py",
            "road_surface_model.py",
            "road_surface_lod.py",
            "road_surface_frames.py",
        )
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, files)], text=True
    ):
        raise ValueError("Freeze measured sources first")
    create_output(args.output)
    row, manifest = read_row(args.ordinal)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files
        },
        "platform": platform.platform(),
        "ordinal": row["ordinal"],
        "form": row["form"],
        "archive_sha256": manifest["archive_sha256"],
        "selected_raw_jsonl_sha256_observed": manifest[
            "selected_raw_jsonl_sha256_observed"
        ],
        "range_m": row["range_m"],
        "section_m": row["section_m"],
        "source_spans": len(row["intervals"]),
        "method": args.method,
        "position_limit_m": args.error_m,
        "normal_limit_degrees": args.angle_deg,
        "max_queries": args.max_queries,
        "max_vertices": args.max_vertices,
        "scope": __doc__,
    }
    began = time.process_time()
    try:
        plan, curve = generate(
            row,
            args.method,
            args.error_m,
            np.deg2rad(args.angle_deg),
            args.max_queries,
            args.max_vertices,
        )
    except ValueError as error:
        report.update(accepted=False, retained_original=True, reason=str(error))
        if isinstance(error, NormalBudget):
            report["work"] = error.work
    else:
        report["generation_single_cpu_s"] = time.process_time() - began
        arrays = {
            key: value for key, value in plan.items() if isinstance(value, np.ndarray)
        }
        images, observed = diagnostic(plan, curve)
        np.savez_compressed(args.output / "plan.npz", **arrays, **images)
        report.update(
            accepted=True,
            retained_original=False,
            generation={
                key: value
                for key, value in plan.items()
                if not isinstance(value, np.ndarray)
            },
            diagnostic=observed,
            vertices=len(plan["positions_enh_m"]),
            triangles=len(plan["indices"]),
            held_plan_array_bytes=sum(value.nbytes for value in arrays.values()),
            cost_scope="Generation CPU only; input read, diagnostic, compression and export excluded. Held plan arrays include chart/query support. Total allocations/copied bytes/peak are not measured here.",
        )
    if "generation_single_cpu_s" not in report:
        report["generation_single_cpu_s"] = time.process_time() - began
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: report[key]
                for key in ("ordinal", "method", "accepted", "generation_single_cpu_s")
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ordinal", type=int, required=True)
    parser.add_argument(
        "--method", choices=("position-only", "adaptive", "uniform"), required=True
    )
    parser.add_argument("--error-m", type=float, default=0.25)
    parser.add_argument("--angle-deg", type=float, default=4.0)
    parser.add_argument("--max-queries", type=int, default=16384)
    parser.add_argument("--max-vertices", type=int, default=65536)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
