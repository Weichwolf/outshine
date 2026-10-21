"""Plan metadata, scan/tree queries and existing detail residency; no native policy."""

import argparse
import hashlib
import json
import platform
import resource
import subprocess
import sys
import tempfile
import time
import tracemalloc
from dataclasses import replace
from pathlib import Path

import numpy as np
import shapely
from compare_window_detail_demand import fingerprint, full_draw, requests
from industrial_relief_probe import camera
from podium_probe import write
from terraced_admission_probe import controls
from terraced_facade_prepare import prepare
from terraced_facade_probe import materials
from terraced_mass_plan import plan
from terraced_mass_sources import sources
from test_window_extent_query import scalar_ids
from window_detail_demand import (
    WindowDetails,
    buffer_bytes,
    materialize,
    window_closed,
    window_patch,
)
from window_extent_plan import compile_bounds
from window_extent_query import WindowRange


def measured(call, trace):
    if trace:
        tracemalloc.start()
    start = time.perf_counter()
    value = call()
    seconds = time.perf_counter() - start
    peak = tracemalloc.get_traced_memory()[1] if trace else None
    if trace:
        tracemalloc.stop()
    return value, {"seconds": seconds, "incremental_traced_peak_bytes": peak}


def query_case(prepared, index, args, jobs):
    metadata, cost = measured(lambda: compile_bounds(prepared), args.trace)
    prefix = f"case-{index}"
    np.save(args.output / (prefix + "-bounds.npy"), metadata.boxes)
    (args.output / (prefix + "-ids.json")).write_text(
        json.dumps(metadata.identifiers) + "\n"
    )
    poses = [replace(view, width=1280, height=720) for view in requests(prepared)[0]]
    choices, rows = {}, {}
    order = (True, False) if bool(index % 2) ^ args.selective_first else (False, True)
    for indexed in order:
        name = "tree" if indexed else "scan"
        selector, build = measured(
            lambda indexed=indexed: WindowRange(metadata, indexed=indexed), args.trace
        )
        samples, total_bounds, total_exact, selected = [], 0, 0, []
        for view in poses:
            # Query loops measure only immutable metadata. Mesh work follows later.
            result, query_cost = measured(
                lambda selector=selector, view=view: selector.query(view.eye, 5.0),
                args.trace,
            )
            selected.append(result.identifiers)
            total_bounds += result.tested_bounds
            total_exact += result.rational_tests
            if args.verify and result.identifiers != scalar_ids(
                metadata, view.eye, 5.0
            ):
                raise ValueError(
                    "Spatial query differs from the scalar rational oracle"
                )
            for _ in range(args.query_loops):
                start = time.perf_counter()
                repeated = selector.query(view.eye, 5.0)
                samples.append(time.perf_counter() - start)
                if repeated.identifiers != result.identifiers:
                    raise ValueError("Immutable query changed on repeated demand")
        rows[name] = {
            "build": build,
            "query_calls": len(samples),
            "query_seconds": samples,
            "median_query_seconds": float(np.median(samples)),
            "seven_state_tested_bounds": total_bounds,
            "seven_state_rational_tests": total_exact,
            "selected_ids": selected,
            "last_single_query": query_cost,
        }
        choices[name] = selected
    if choices["scan"] != choices["tree"]:
        raise ValueError("Index changed stable source-plan selection order")
    escaped, native_bbox_extra = 0, 0
    if args.verify:
        for window, box in zip(prepared.facade.windows, metadata.boxes, strict=True):
            actual = window_closed(prepared, window)["vertices"]
            if np.any(actual < box[0]) or np.any(actual > box[1]):
                escaped += 1
            # Stored boxes are conservative envelopes, not exact physical extents.
            physical = np.stack((actual.min(0), actual.max(0)))
            for view, selected in zip(poses, choices["scan"], strict=True):
                gap = np.maximum(
                    np.maximum(physical[0] - view.eye, view.eye - physical[1]), 0
                )
                if window.identifier in selected and gap @ gap > 25:
                    native_bbox_extra += 1
        if escaped:
            raise ValueError("Actual closed-window geometry escaped plan metadata")
    for view, selected in zip(poses, choices["scan"], strict=True):
        jobs.append(
            {
                "case": index,
                "center": view.eye.tolist(),
                "radius_m": 5.0,
                "ids": selected,
            }
        )
    return (
        {
            "source_id": prepared.mass.identifier,
            "kind": prepared.mass.kind,
            "windows": len(metadata.boxes),
            "acquisition": cost,
            "bounds_bytes": metadata.boxes.nbytes,
            "id_utf8_bytes": sum(len(s.encode()) for s in metadata.identifiers),
            "escaped_window_bodies": escaped if args.verify else None,
            "extra_selections_against_actual_mesh_aabbs": native_bbox_extra
            if args.verify
            else None,
            "methods": rows,
        },
        poses,
        choices["scan"],
    )


def lifecycle(prepared, poses, selected, all_details, args):
    product, coarse = measured(lambda: WindowDetails(prepared), args.trace)
    full, full_cost = (
        measured(
            lambda: {
                w.identifier: window_patch(prepared, w) for w in prepared.facade.windows
            },
            args.trace,
        )
        if all_details
        else ({}, {"seconds": 0.0, "incremental_traced_peak_bytes": 0})
    )
    rows, snapshots, last = [], {}, None
    for index, ids in enumerate(selected):
        draw, cost = measured(
            lambda ids=ids: (
                full_draw(product, full, ids) if all_details else product.update(ids)
            ),
            args.trace,
        )
        rows.append(
            {
                "cost": cost,
                "detailed_ids": draw.detailed_ids,
                "generated_ids": draw.generated_ids,
                "released_ids": draw.released_ids,
                "fallback_ids": draw.fallback_ids,
                "generated_buffer_bytes": draw.generated_buffer_bytes,
                "resident_draw_detail_bytes": draw.resident_detail_bytes,
            }
        )
        last = draw
        if index in (0, len(selected) - 1):
            snapshots[index] = materialize(draw)
    name = "all" if all_details else "selective"
    for index, mesh in snapshots.items():
        np.savez_compressed(args.output / f"{name}-state-{index}.npz", **mesh)
    final = snapshots[len(selected) - 1]
    write(
        args.output,
        name,
        final,
        {
            "camera": camera(poses[-1]),
            "source_id": prepared.mass.identifier,
            "source": {"material_definitions": materials()},
            "facade_materials": {},
            "scope": "5m window-AABB radius requests; complete existing compact exterior retained. Same requested products in all/selective residency. No visibility/radiance/PBR/native LOD/device admission.",
        },
        {"producer": args.producer},
    )
    return {
        "coarse_construction": coarse,
        "coarse_bytes": buffer_bytes(product.coarse.mesh),
        "all_initial_detail_cost": full_cost,
        "initial_full_windows_generated": len(full),
        "updates": rows,
        "retained_detail_bytes": sum(buffer_bytes(p.mesh) for p in full.values())
        if all_details
        else last.resident_detail_bytes,
        "snapshot_hashes": {str(i): fingerprint(mesh) for i, mesh in snapshots.items()},
        "scope": "Previous draw is held through an update, then released. Snapshot materialization/serialization is diagnostic, outside update timers. No full physical-building timing control.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--query-loops", type=int, default=32)
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--selective-first", action="store_true")
    args = parser.parse_args()
    temp = Path(tempfile.gettempdir()).resolve()
    if (
        args.output.exists()
        or not args.output.resolve().is_relative_to(temp)
        or args.output.resolve() == temp
        or not 1 <= args.query_loops <= 1024
    ):
        parser.error(
            "Fresh explicit system-temp output and bounded query loops required"
        )
    tracked = [
        "test/experiments/buildings/" + name
        for name in (
            "window_extent_query.py",
            "window_extent_plan.py",
            "test_window_extent_query.py",
            "compare_window_extents.py",
            "window_detail_demand.py",
            "opening_modules.py",
            "terraced_facade_details.py",
        )
    ]
    tracked += [
        "test/experiments/buildings/" + name
        for name in (
            "opening_details.py",
            "terraced_facade_prepare.py",
            "terraced_facade_surface.py",
            "compare_window_detail_demand.py",
            "data/stepped-profiles.json",
            "data/terraced-admission-controls.json",
        )
    ]
    if subprocess.check_output(["git", "status", "--porcelain", "--", *tracked]):
        parser.error("Freeze metadata/query/recipe sources before comparing costs")
    args.producer = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True
    ).strip()
    args.output.mkdir(parents=True)
    original = sources()
    compact = next(row for row in controls() if row["shape"] == "compact")
    start = time.perf_counter()
    prepared = [
        prepare(plan(row, kind))
        for row in (*original, compact)
        for kind in ("cascade", "paired-wings")
    ]
    preparation = time.perf_counter() - start
    jobs, rows, poses, choices = [], [], [], []
    for index, source in enumerate(prepared):
        row, views, ids = query_case(source, index, args, jobs)
        rows.append(row)
        poses.append(views)
        choices.append(ids)
    residency = {}
    for all_details in (False, True) if args.selective_first else (True, False):
        residency["all" if all_details else "selective"] = lifecycle(
            prepared[0], poses[0], choices[0], all_details, args
        )
    if residency["all"]["snapshot_hashes"] != residency["selective"]["snapshot_hashes"]:
        raise ValueError("Same radius requests produced different complete draw fields")
    (args.output / "query-jobs.json").write_text(json.dumps(jobs, indent=2) + "\n")
    report = {
        "input_sha256": {
            name: hashlib.sha256(
                (Path(__file__).with_name("data") / name).read_bytes()
            ).hexdigest()
            for name in ("stepped-profiles.json", "terraced-admission-controls.json")
        },
        "producer": args.producer,
        "verify": args.verify,
        "trace": args.trace,
        "python": platform.python_version(),
        "numpy": np.__version__,
        "shapely": shapely.__version__,
        "geos": shapely.geos_version_string,
        "plan_preparation_seconds": preparation,
        "cases": rows,
        "residency_case": residency,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "scope": "Linux shared-host Python, one BLAS thread; explicit 5m AABB-radius requests, not visibility/occlusion/radiance/LOD admission. Both selectors read same saved-capable immutable metadata. Geometry controls, query loops, draw materialization/exports outside timed acquisition/index/single-query/detail phases. All control retains each closed patch once and draws the same requested subset, not full physical facade; existing batch is cheaper than per-window full expansion. Coarse/model generation not free. Trace excludes some native GEOS heaps; process RSS includes diagnostics and imports. No actual DB/IO/upload/native frame/application memory claim.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "cases": len(rows),
                "windows": sum(r["windows"] for r in rows),
                "output": str(args.output),
            }
        )
    )


if __name__ == "__main__":
    main()
