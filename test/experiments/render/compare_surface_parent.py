"""Compare actual SHA-bound native geometry with certified geometry-only parents."""

import argparse
import hashlib
import json
import platform
import resource
import tempfile
import time
import tracemalloc
from collections import Counter
from pathlib import Path

import numpy as np
from check_surface_parent import check_parent
from clustered_surface_parent import cluster_parent
from facing_inputs import buildings
from far_product_cut import projection_error


def measure(model, size, *, allocations):
    xyz, ids = model["vertices"], model["indices"]
    if allocations:
        tracemalloc.start()
    started = time.perf_counter()
    parent = cluster_parent(xyz, ids, cell_m=size)
    seconds = time.perf_counter() - started
    allocation = None
    if allocations:
        current, peak = tracemalloc.get_traced_memory()
        snapshot = tracemalloc.take_snapshot()
        allocation = {
            "live_traced_bytes": current,
            "peak_traced_bytes": peak,
            "live_traced_blocks": sum(s.count for s in snapshot.statistics("filename")),
        }
        tracemalloc.stop()
    checked = check_parent(xyz, ids, parent)
    points = xyz[np.unique(ids)].astype(float)
    box = np.stack((points.min(0), points.max(0)))
    scene = model["scene"]
    eye = np.array(scene["eye_ecef"]) - scene["anchor_ecef"]
    observer = np.stack((eye - 64, eye + 64))
    projected = projection_error(
        box,
        observer,
        displacement_m=parent.error_m,
        focal_px=scene["focal_px"],
        width=1280,
        height=720,
    )
    useful = len(parent.triangles) < len(ids) and projected <= 1
    return {
        "key": model["key"],
        "cohort": model["group"],
        "cell_m": size,
        "provenance": model["provenance"],
        "work": parent.work,
        "geometry_error_upper_m": parent.error_m,
        "pixel_error_upper": float(projected) if np.isfinite(projected) else None,
        "observer_box": observer.tolist(),
        "profile": [1280, 720, scene["focal_px"], 1],
        "geometry_bound_passes": bool(useful),
        "selected_triangles_geometry_only": len(parent.triangles)
        if useful
        else len(ids),
        "python_bake_seconds": seconds,
        "allocation": allocation,
        "independent_check": checked,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cell-m", nargs="+", type=float, default=[0.125, 0.5, 2.0])
    parser.add_argument("--limit", type=int)
    parser.add_argument("--allocations", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("private output must be in system temporary directory")
    output.mkdir(parents=True, exist_ok=False)
    rows = []
    started = time.perf_counter()
    for number, model in enumerate(buildings(args.inputs)):
        if args.limit is not None and number >= args.limit:
            break
        for size in args.cell_m:
            row = measure(model, size, allocations=args.allocations)
            rows.append(row)
            with (output / "rows.jsonl").open("a") as f:
                f.write(json.dumps(row, allow_nan=False) + "\n")
        if number % 25 == 0:
            print(
                json.dumps(
                    {"streams": number + 1, "seconds": time.perf_counter() - started}
                ),
                flush=True,
            )
    totals = {}
    for row in rows:
        key = row["cohort"] + ":" + str(row["cell_m"])
        total = totals.setdefault(key, Counter())
        work = row["work"]
        total.update(
            streams=1,
            reference_triangles=work["reference_triangles"],
            parent_triangles=work["parent_triangles"],
            selected_triangles_geometry_only=row["selected_triangles_geometry_only"],
            geometry_bound_passes=int(row["geometry_bound_passes"]),
            original_vertices=work["input_vertices"],
            parent_vertices=work["parent_vertices"],
            unchanged_triangles=work["unchanged_triangles"],
            covered_collapses=work["covered_collapses"],
            duplicate_images=work["duplicate_images"],
            corner_tests=work["mapped_corner_distance_tests"],
            parent_position_bytes=work["parent_position_bytes"],
            parent_index_bytes_u32=work["parent_index_bytes_u32"],
            reference_geometry_bytes=work["reference_geometry_bytes"],
            python_bake_seconds=row["python_bake_seconds"],
        )
    hashes = {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in (
            Path(__file__),
            Path(__file__).with_name("clustered_surface_parent.py"),
            Path(__file__).with_name("check_surface_parent.py"),
            Path(__file__).with_name("far_product_cut.py"),
        )
    }
    report = {
        "scope": "Geometry-only offline parents of original cached streams, not source-complete native parent products. Original product/cell catalogs retained; no per-triangle source/material inference. Selection here happens after decode for ROI only. No topology, coverage, attributes, shadows, GPU or device admission.",
        "input_manifest_sha256": hashlib.sha256(
            (args.inputs / "manifest.json").read_bytes()
        ).hexdigest(),
        "code_sha256": hashes,
        "rows": len(rows),
        "totals": totals,
        "python": platform.python_version(),
        "platform": platform.platform(),
        "wall_seconds": time.perf_counter() - started,
        "process_peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "allocation_scope": "tracemalloc live blocks/current/peak only when requested; not cumulative allocations or all native allocator traffic",
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n"
    )
    print(json.dumps(report["totals"], indent=2), flush=True)


if __name__ == "__main__":
    main()
