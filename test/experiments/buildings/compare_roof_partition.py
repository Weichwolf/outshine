"""Compare bounded roof recipes against the unchanged offline plans (#955)."""

import argparse
import hashlib
import importlib.metadata
import json
import platform
import resource
import subprocess
import sys
import time
import tracemalloc
from contextlib import contextmanager
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.buildings.modelling import SkeletonLimits, prepare_hip
from outshine_ref.buildings.modelling.roof_partition import prepare
from verify_hip_fixture import verify


@contextmanager
def block_skeletons(method):
    names = (
        (
            "pysfcgal",
            "pysfcgal.geometry",
            "py_straight_skeleton",
            "py_straight_skeleton.algorithm",
        )
        if method == "contour"
        else ()
    )
    absent = object()
    saved = {name: sys.modules.get(name, absent) for name in names}
    try:
        for name in names:
            sys.modules[name] = None
        yield
    finally:
        for name, value in saved.items():
            if value is absent:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = value


def plan_arrays(plan):
    return {
        "xy": plan.positions_m,
        "distance": plan.distances_m,
        "origin": np.array(plan.origin),
        "offsets": np.r_[
            0, np.cumsum([len(f.node_indices) for f in plan.facets])
        ].astype(np.uint32),
        "indices": np.array(
            [i for f in plan.facets for i in f.node_indices], dtype=np.uint32
        ),
        "owners": np.array([f.boundary for f in plan.facets], dtype=np.uint32),
        "planes": np.array([[*f.gradient, f.intercept_m] for f in plan.facets]),
    }


def compare_case(row, tolerance, method):
    work = {}
    prepare_seconds = 0.0

    def producer(polygon):
        nonlocal prepare_seconds
        began = time.perf_counter()
        try:
            return (
                prepare(polygon, work=work)
                if method == "contour"
                else prepare_hip(polygon, method)
            )
        finally:
            prepare_seconds = time.perf_counter() - began

    result = {"id": row["id"], "method": method, "holes": len(row["holes"])}
    began = time.perf_counter()
    arrays = {}
    try:
        with block_skeletons(method):
            plan = verify(row, tolerance, producer)
        arrays = plan_arrays(plan)
        result.update(
            accepted=True,
            nodes=len(plan.positions_m),
            facets=len(plan.facets),
            maximum_total_refinement_bound_m=plan.maximum_refinement_m,
            diagnostic_array_bytes=sum(a.nbytes for a in arrays.values()),
        )
    except (
        ValueError,
        RuntimeError,
        ArithmeticError,
        AssertionError,
        ImportError,
    ) as error:
        result.update(accepted=False, error=type(error).__name__ + ": " + str(error))
    result.update(
        prepare_seconds=prepare_seconds,
        total_check_seconds=time.perf_counter() - began,
        work=work,
    )
    return result, arrays


def offset_alternative(row):
    expected = row["expected"]
    nodes = np.array(expected["positions_local_xy_m"])
    origin = np.array(row["frame"]["origin_xy_m"])
    source = shapely.Polygon(
        np.array(row["outer"]) - origin, [np.array(r) - origin for r in row["holes"]]
    )
    scale = max(1.0, np.linalg.norm(nodes, axis=1).max()) * 8
    errors = []
    for fraction in (0.05, 0.15, 0.3, 0.5, 0.7, 0.9, 0.99):
        distance = max(expected["wavefront_distances_m"]) * fraction
        pieces = []
        for facet in expected["facets"]:
            gradient = np.array(facet["gradient"])
            axis = np.array((-gradient[1], gradient[0]))
            base = gradient * (distance - facet["intercept_m"])
            halfplane = shapely.Polygon(
                [
                    base - axis * scale,
                    base + axis * scale,
                    base + axis * scale + gradient * scale,
                    base - axis * scale + gradient * scale,
                ]
            )
            pieces.append(
                shapely.Polygon(nodes[facet["nodes"]]).intersection(halfplane)
            )
        control = shapely.union_all(pieces)
        candidate = shapely.buffer(
            source, -distance, join_style="mitre", mitre_limit=1e8, quad_segs=1
        )
        errors.append(control.symmetric_difference(candidate).area)
    return {
        "id": row["id"],
        "sampled_level_set_area_errors_m2": errors,
        "maximum_area_error_m2": max(errors),
    }


def allocation_diagnostic(row):
    polygon = shapely.Polygon(row["outer"], row["holes"])
    tracemalloc.start()
    began = time.perf_counter()
    try:
        plan = prepare(polygon, limits=SkeletonLimits(seconds=15))
        current, peak = tracemalloc.get_traced_memory()
        snapshot = tracemalloc.take_snapshot()
    finally:
        tracemalloc.stop()
    return {
        "id": row["id"],
        "diagnostic_allowance_seconds": 15,
        "seconds": time.perf_counter() - began,
        "python_tracked_current_bytes": current,
        "python_tracked_peak_bytes": peak,
        "retained_tracked_blocks": len(snapshot.traces),
        "nodes": len(plan.positions_m),
        "scope": "Allocation-only repeat; not default2s admission or native/GEOS total peak.",
    }


def run(args):
    fixture = json.loads(args.fixture.read_text())
    assert fixture["schema"] == "outshine.hip-plan-reference/1"
    assert len(fixture["records"]) == 48
    args.output.mkdir(parents=True, exist_ok=False)
    root = Path(__file__).resolve().parents[3]
    paths = [
        Path(__file__).resolve(),
        Path(__file__).with_name("verify_hip_fixture.py"),
        *sorted((root / "lib/src/outshine_ref/buildings/modelling").glob("*roof*.py")),
        root / "lib/src/outshine_ref/buildings/modelling/skeleton_plan.py",
        root / "lib/src/outshine_ref/geometry/welding.py",
    ]
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=root, text=True
        ).strip(),
        "working_tree_dirty": bool(
            subprocess.check_output(
                [
                    "git",
                    "status",
                    "--porcelain",
                    "--",
                    "lib",
                    "test/experiments/buildings",
                ],
                cwd=root,
                text=True,
            )
        ),
        "implementation_sha256": {
            str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in paths
        },
        "fixture_sha256": hashlib.sha256(args.fixture.read_bytes()).hexdigest(),
        "fixture_file_bytes": args.fixture.stat().st_size,
        "python": platform.python_version(),
        "platform": platform.platform(),
        "packages": {
            name: importlib.metadata.version(name)
            for name in ("numpy", "scipy", "shapely")
        },
        "default_limits": {
            "vertices": 256,
            "nodes": 1024,
            "events": 4096,
            "seconds": 2,
        },
        "scope": "Pinned roof geometry/ownership/height modes; no native/device/GPU/world admission.",
        "rows": [],
    }
    arrays = {}
    for method in args.methods:
        for index, row in enumerate(fixture["records"]):
            result, values = compare_case(row, fixture["comparison"], method)
            report["rows"].append(result)
            if method == "contour":
                arrays.update(
                    {f"case_{index:02d}_{key}": value for key, value in values.items()}
                )
        selected = [r for r in report["rows"] if r["method"] == method]
        print(
            method,
            sum(r["accepted"] for r in selected),
            "/",
            len(selected),
            "prepare_seconds",
            sum(r["prepare_seconds"] for r in selected),
            flush=True,
        )
    if args.trace_allocations:
        biggest = max(
            fixture["records"],
            key=lambda r: len(r["outer"]) + sum(map(len, r["holes"])),
        )
        report["allocation_diagnostic"] = allocation_diagnostic(biggest)
    if args.check_offset_alternative:
        report["mitered_offset_alternative"] = [
            offset_alternative(row) for row in fixture["records"]
        ]
    peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    report["whole_process_peak_rss_bytes"] = peak * (
        1 if sys.platform == "darwin" else 1024
    )
    report["rss_scope"] = (
        "Whole diagnostic process including dependencies, oracle fixture, plans and checks."
    )
    report["diagnostic_arrays_bytes"] = sum(a.nbytes for a in arrays.values())
    report["unmeasured"] = [
        "native device time",
        "GPU time",
        "native/GEOS allocation count",
        "all allocation/copy/traffic bytes",
        "native serialized asset bytes",
    ]
    np.savez_compressed(args.output / "plans.npz", **arrays)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    gated = (
        [r for r in report["rows"] if r["method"] == "contour"]
        if "contour" in args.methods
        else report["rows"]
    )
    return all(r["accepted"] for r in gated)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixture",
        type=Path,
        default=Path(__file__).with_name("reference") / "hip-plans.json",
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--methods",
        nargs="+",
        choices=("contour", "skeleton", "auto"),
        default=("contour", "skeleton", "auto"),
    )
    parser.add_argument("--trace-allocations", action="store_true")
    parser.add_argument("--check-offset-alternative", action="store_true")
    raise SystemExit(0 if run(parser.parse_args()) else 1)
