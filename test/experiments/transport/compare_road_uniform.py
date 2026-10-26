"""Equal-envelope adaptive/uniform upper surfaces on the complete native corpus."""

import argparse
import hashlib
import json
import lzma
import platform
import resource
import subprocess
import sys
import tempfile
import time
import tracemalloc
from pathlib import Path

import numpy as np
from road_surface_checks import metadata_error, observed_error
from road_surface_lod import prepare, prepare_uniform
from road_surface_model import Curve

ARRAYS = ("stations_m", "positions_enh_m", "indices")
METHODS = {"adaptive": prepare, "uniform": prepare_uniform}


def owners(values):
    roots = {}
    for value in values:
        if isinstance(value, np.ndarray):
            while isinstance(value.base, np.ndarray):
                value = value.base
            roots[id(value)] = value
    return {
        "owner_buffers": len(roots),
        "owner_bytes": sum(a.nbytes for a in roots.values()),
    }


def generate(recipe, method, error, trace):
    if trace:
        tracemalloc.start()
    began = time.perf_counter()
    curve = Curve(recipe)
    prepared = time.perf_counter()
    plan = method(curve, error)
    ended = time.perf_counter()
    peak = tracemalloc.get_traced_memory()[1] if trace else None
    if trace:
        tracemalloc.stop()
    report = {k: v for k, v in plan.items() if k not in ARRAYS}
    report.update(
        generation_seconds=ended - began,
        model_seconds=prepared - began,
        subdivision_and_mesh_seconds=ended - prepared,
        traced_peak_bytes=peak,
        stations=len(plan["stations_m"]),
        triangles=len(plan["indices"]),
        geometry_payload_bytes=plan["positions_enh_m"].nbytes + plan["indices"].nbytes,
        retained_plan_arrays=owners(plan.values()),
        retained_model_arrays=owners(vars(curve).values()),
        array_sha256={k: hashlib.sha256(plan[k].tobytes()).hexdigest() for k in ARRAYS},
    )
    if report["maximum_stored_bound_m"] > error:
        raise ValueError("Candidate exceeds the shared stored-surface bound")
    return curve, plan, report


def compare_row(row, args, arrays, pin):
    recipe = {key: row[key] for key in ("range_m", "section_m", "seams", "intervals")}
    result = {
        "ordinal": row["ordinal"],
        "form": row["form"],
        "source_intervals": len(recipe["intervals"]),
        "methods": {},
    }
    names = list(METHODS)
    if bool(row["ordinal"] % 2) ^ args.uniform_first:
        names.reverse()
    result["measurement_order"] = names
    for name in names:
        curve, plan, report = generate(recipe, METHODS[name], args.error_m, args.trace)
        if pin is not None and name == "adaptive":
            for key in ARRAYS:
                original = pin[f"curve_{row['ordinal']}_{key}"]
                if (
                    original.dtype != plan[key].dtype
                    or original.shape != plan[key].shape
                    or original.tobytes() != plan[key].tobytes()
                ):
                    raise ValueError(
                        "Adaptive refactor changed the frozen complete product"
                    )
            report["original_adaptive_pin_equal"] = True
        if args.verify:
            before = time.perf_counter()
            center, direction = metadata_error(curve)
            error, witness, tested = observed_error(
                curve, plan["stations_m"], plan["positions_enh_m"]
            )
            if (
                center > 1e-7
                or direction > 1e-8
                or error > report["maximum_stored_bound_m"] + 1e-7
            ):
                raise ValueError(
                    "Independent metadata/sample control exceeds the existing model contract"
                )
            report.update(
                offline_verification_seconds=time.perf_counter() - before,
                observed_error_m=error,
                witness=witness,
                offline_sample_stations=tested,
                midpoint_center_error_m=center,
                midpoint_direction_error=direction,
            )
        for key in ARRAYS:
            arrays[f"{name}_{row['ordinal']}_{key}"] = plan[key]
        result["methods"][name] = report
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--error-m", type=float, default=0.25)
    parser.add_argument("--adaptive-pin", type=Path)
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--uniform-first", action="store_true")
    args = parser.parse_args()
    temp = Path(tempfile.gettempdir()).resolve()
    if (
        args.output.exists()
        or not args.output.resolve().is_relative_to(temp)
        or args.output.resolve() == temp
    ):
        parser.error("Fresh explicit system-temp output required")
    if not np.isfinite(args.error_m) or args.error_m <= 0:
        parser.error("Finite positive metric error required")
    tracked = [
        str(Path(__file__).with_name(name).relative_to(Path.cwd()))
        for name in (
            "road_surface_lod.py",
            "road_surface_model.py",
            "road_surface_checks.py",
            "compare_road_uniform.py",
            "test_road_surface_uniform.py",
            "test_road_surface_lod.py",
        )
    ]
    if subprocess.check_output(["git", "status", "--porcelain", "--", *tracked]):
        parser.error("Freeze all surface sources before comparing costs")
    folder = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/transport/road_curve_requests"
    )
    manifest = json.loads((folder / "manifest.json").read_text())
    archive = folder / manifest["archive"]
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != manifest["archive_sha256"]:
        raise ValueError("Original archive digest changed")
    args.output.mkdir(parents=True)
    pin = np.load(args.adaptive_pin) if args.adaptive_pin else None
    rows, arrays = [], {}
    started = time.perf_counter()
    with lzma.open(archive, "rt") as stream:
        for line in stream:
            row = json.loads(line)
            rows.append(compare_row(row, args, arrays, pin))
            if len(rows) % 100 == 0:
                print("compared", len(rows), flush=True)
    if len(rows) != manifest["records"]:
        raise ValueError("Incomplete original corpus")
    elapsed = time.perf_counter() - started
    if pin is not None:
        pin.close()
    np.savez_compressed(args.output / "plans.npz", **arrays)
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "archive_sha256": digest,
        "manifest_sha256": hashlib.sha256(
            (folder / "manifest.json").read_bytes()
        ).hexdigest(),
        "adaptive_pin_sha256": hashlib.sha256(
            args.adaptive_pin.read_bytes()
        ).hexdigest()
        if args.adaptive_pin
        else None,
        "error_m": args.error_m,
        "verify": args.verify,
        "trace": args.trace,
        "python": platform.python_version(),
        "numpy": np.__version__,
        "elapsed_compare_seconds": elapsed,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "scope": "Shared-host Python; one BLAS thread. Both methods include identical analytic model preparation and share the piecewise surface envelope, twist and Float32 rounding threshold. Build does not read emitted Upper arrays. Source IO/decode, optional frozen-pin reads, offline verification and output serialization are outside individual build stages. Traced peak includes one candidate's model and mesh, not application residency; process RSS includes both corpus outputs. Upper geometry only: materials/normals/UV/ports/junctions/walls/caps/coverage/physical validity/native devices remain unqualified. Uniform is first qualifying global dyadic grid, no all-N/global-optimality claim.",
        "rows": rows,
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print("complete", len(rows), flush=True)


if __name__ == "__main__":
    main()
