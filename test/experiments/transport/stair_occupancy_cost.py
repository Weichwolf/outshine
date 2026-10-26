"""Identical actual footprint/probe queries; preparation and optional allocations separate."""

import argparse
import hashlib
import json
import resource
import time
from pathlib import Path

import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import BuildingFootprint, HeightInterval
from outshine_ref.pipeline import adjusted_footprints
from stair_occupancy_handoff import OccupancyQuery, adjusted
from stair_occupancy_inputs import actual_sources, buildings, full_tile_sampler
from stair_occupancy_verify import read
from stair_terrain_experiment import frozen


def measured(query, footprints, method):
    began = time.perf_counter()
    changes, notes = adjusted(query, footprints, method=method)
    parts = adjusted_footprints(footprints, changes)
    result = hashlib.sha256(b"".join(p.geometry.wkb for p in parts)).hexdigest()
    return {
        "seconds": time.perf_counter() - began,
        "result_geometry_sha256": result,
        "query_rows": sum(n["query_rows"] for n in notes),
        "xy_candidates": sum(n["xy_candidates"] for n in notes),
        "height_candidates": sum(n["height_candidates"] for n in notes),
    }


def run(args):
    commit = frozen()
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("private occupancy cost outputs must live under /tmp")
    args.output.mkdir(parents=True, exist_ok=False)
    products = args.products
    report = json.loads((products / "comparison.json").read_text())
    verified = json.loads((products / "verification.json").read_text())
    if (
        verified["products_report_sha256"] != digest(products / "comparison.json")
        or not verified["valid_saved_occupancy"]
    ):
        raise ValueError("verified same-input occupancy products required")
    for name, expected in report["artifacts"].items():
        if digest(products / name) != expected:
            raise ValueError("cost input changed: " + name)
    context, _sources, _sample, _layer = actual_sources()
    footprints, _ledger, _receipt = buildings(
        context, full_tile_sampler(Path(report["dem"]["path"]), context)
    )
    probes = tuple(
        BuildingFootprint(
            r["id"],
            shapely.from_wkb(r["geometry_wkb"]),
            HeightInterval(*r["heights_m"]),
        )
        for r in json.loads((products / "probes.json").read_text())
    )
    inputs = footprints + probes
    began = time.perf_counter()
    rows, _data = read(products / "volumes.json", report)
    query = OccupancyQuery(rows)
    preparation = time.perf_counter() - began
    results = {method: [] for method in ("scan", "index")}
    for i in range(3):
        for method in ("scan", "index") if i % 2 == 0 else ("index", "scan"):
            results[method].append(measured(query, inputs, method))
    hashes = {r["result_geometry_sha256"] for rows in results.values() for r in rows}
    if len(hashes) != 1:
        raise ValueError("paired complete spatial handoff results differ")
    profiles = {}
    if args.profile:
        import memray

        for method in ("scan", "index"):
            path = args.output / (method + ".bin")
            with memray.Tracker(
                str(path), trace_python_allocators=True, native_traces=False
            ):
                actual = measured(query, inputs, method)
            if actual["result_geometry_sha256"] not in hashes:
                raise ValueError("allocation-profiled handoff differs")
            profiles[method] = {
                "path": str(path),
                "sha256": digest(path),
                "profiled_seconds": actual["seconds"],
            }
    result = {
        "commit": commit,
        "products_report_sha256": digest(products / "comparison.json"),
        "preparation_seconds": preparation,
        "query_count": len(inputs),
        "volume_count": len(rows),
        "normal_priority_paired_runs": results,
        "allocation_profiles": profiles,
        "explicit_query_shape_reference_array_bytes": query.shapes.nbytes,
        "input_geometry_wkb_bytes": sum(len(p.geometry.wkb) for p in inputs),
        "serialized_volume_bytes": (products / "volumes.json").stat().st_size,
        "copied_geometry_bytes": None,
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Source preload and immutable geometry decode outside query timers/traces; index setup separate. Query rows are exposed candidates, internal tree visits unmeasured. Explicit array bytes are references, not complete copies. GEOS-internal copied bytes unmeasured. Memray optional; instrumented times not normal device timings.",
    }
    (args.output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", action="store_true")
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    run(parser.parse_args())
