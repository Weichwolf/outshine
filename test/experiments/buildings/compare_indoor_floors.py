"""Offline floor-programme comparison on pinned original provider inputs (#111)."""

import argparse
import hashlib
import json
import os
import platform
import resource
import subprocess
import time
from collections import Counter
from dataclasses import asdict
from pathlib import Path

import numpy as np
import shapely
from indoor_floor_geometry import produce_floor_contacts
from indoor_floor_inputs import load_inputs
from indoor_floor_plan import FloorLimits

METHODS = ("uniform", "structural", "constrained", "adapted")
DATA = (
    Path(__file__).resolve().parents[2]
    / "outshine/data/buildings/indoor_floor_requests"
)


def json_value(value):
    if isinstance(value, np.ndarray):
        return value.tolist()
    if isinstance(value, np.generic):
        return value.item()
    raise TypeError(type(value).__name__)


def allocations(path, call):
    import memray

    with memray.Tracker(path, trace_python_allocators=True, native_traces=True):
        value = call()
    reader = memray.FileReader(path)
    frees = {
        memray.AllocatorType.FREE,
        memray.AllocatorType.PYMALLOC_FREE,
        memray.AllocatorType.MUNMAP,
    }
    count, requested = 0, 0
    for event in reader.get_allocation_records():
        if event.allocator not in frees and event.size > 0:
            count += event.n_allocations
            requested += event.size
    return value, {
        "allocation_events": count,
        "requested_bytes": requested,
        "scoped_high_watermark_bytes": sum(
            r.size for r in reader.get_high_watermark_allocation_records()
        ),
        "profiler": f"memray {memray.__version__}",
        "trace": path.name,
    }


def digest(result):
    value = hashlib.sha256()
    value.update(
        json.dumps(result["choices"], sort_keys=True, default=json_value).encode()
    )
    for floor in result["floors"]:
        value.update(floor.identifier.encode())
        value.update(floor.polygon.wkb)
    for key, mesh in sorted(result["meshes"].items()):
        value.update(key.encode())
        for name in ("vertices", "faces", "parts"):
            value.update(mesh[name].tobytes())
    return value.hexdigest()


def describe(result):
    return {
        "ready": sum(r["status"] == "ready" for r in result["choices"]),
        "pending_reasons": dict(
            Counter(r["reason"] for r in result["choices"] if r["status"] != "ready")
        ),
        "choices": result["choices"],
        "work": result["work"],
        "programmes": result["programmes"],
        "floors": [
            {
                "id": f.identifier,
                "level": f.level,
                "wkb_hex": f.polygon.wkb_hex,
                "lower_z_m": f.lower_z_m,
                "upper_z_m": f.upper_z_m,
                "source_ids": f.source_ids,
            }
            for f in result["floors"]
        ],
        "mesh_audits": result["mesh_audits"],
        "mesh_failures": result["mesh_failures"],
        "outline_adaptation": result["outline_adaptation"],
        "mesh_payload_bytes": sum(
            a["payload_bytes"] for a in result["mesh_audits"].values()
        ),
        "contact_xyz_bytes": sum(
            r["xyz"].nbytes for r in result["choices"] if "xyz" in r
        ),
        "floor_model_wkb_bytes": sum(len(f.polygon.wkb) for f in result["floors"]),
        "reservations": [
            {
                "owner_id": r.owner_id,
                "wkb_hex": r.geometry.wkb_hex,
                "height_interval_m": asdict(r.heights),
                "purpose": r.purpose,
            }
            for r in result["reservations"]
        ],
        "determinism_sha256": digest(result),
    }


def compare(root, output, rounds, trace=False, plot=False):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=False)
    cpu, wall = time.process_time(), time.perf_counter()
    inputs = load_inputs(root)
    source_cost = {
        "wall_s": time.perf_counter() - wall,
        "cpu_s": time.process_time() - cpu,
        "stored_source_and_manifest_bytes": (Path(root) / "manifest.json")
        .stat()
        .st_size
        + sum(
            (Path(root) / r["archive"]).stat().st_size
            for r in inputs.metadata["source_receipts"]
        ),
        "scope": "Offline receipt verification, source read/decompression/decoding, ENU normalization/index input and DEM raster preload. No internet acquisition or asset-cache hit.",
    }
    limits = FloorLimits()

    def generate(method):
        return produce_floor_contacts(
            inputs.parts,
            inputs.requests,
            inputs.ground,
            method=method,
            limits=limits,
            constraints=inputs.constraints,
        )

    results = {method: generate(method) for method in METHODS}
    hashes = {m: digest(r) for m, r in results.items()}
    samples = {m: [] for m in METHODS}
    for turn in range(rounds):
        order = METHODS[turn % len(METHODS) :] + METHODS[: turn % len(METHODS)]
        for method in order:
            cpu, wall = time.process_time(), time.perf_counter()
            result = generate(method)
            samples[method].append(
                {
                    "wall_s": time.perf_counter() - wall,
                    "cpu_s": time.process_time() - cpu,
                }
            )
            if digest(result) != hashes[method]:
                raise ValueError("Floor results changed across equal-input runs")
            del result
    loop_rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    costs = {}
    if trace:
        for method in METHODS:
            value, costs[method] = allocations(
                output / f"{method}.bin", lambda method=method: generate(method)
            )
            if digest(value) != hashes[method]:
                raise ValueError("Profiled and unprofiled floor results differ")
            del value
    arrays = {}
    for method, result in results.items():
        for index, floor in enumerate(result["floors"]):
            if floor.identifier in result["meshes"]:
                for name, array in result["meshes"][floor.identifier].items():
                    arrays[f"{method}/{index}/{name}"] = array
    np.savez(output / "meshes.npz", **arrays)
    report = {
        "schema": "outshine/indoor-floor-comparison/1",
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "worktree_status": subprocess.check_output(
            ["git", "status", "--porcelain"], text=True
        ),
        "platform": platform.platform(),
        "load_average": os.getloadavg(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "geos": shapely.geos_version_string,
        "threads": {
            k: os.environ.get(k) for k in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
        "input": inputs.metadata,
        "source_cost": source_cost,
        "limits": asdict(limits),
        "scope": "Procedural fixed-snapshot source envelopes, chosen floors and closed plates. No public-route/room/door, whole-terrain/statics, native Source/World, cache pipeline or device/frame certificate. Width1.5m, storey/slab/headroom/foundation and outline bounds are explicit recipes, not surveyed facts. Timings include plate generation/checks; separate profiler runs are excluded. Reported payload sizes are emitted arrays/WKB, not total copies or process heap.",
        "generation_loop_process_peak_rss_bytes": loop_rss
        * (1 if platform.system() == "Darwin" else 1024),
        "methods": {
            m: describe(r) | {"runs": samples[m], "allocations": costs.get(m)}
            for m, r in results.items()
        },
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2, default=json_value) + "\n"
    )
    if plot:
        from indoor_floor_plot import draw

        draw(inputs, results, output / "floors.png")
    print(
        json.dumps(
            {
                "output": str(output),
                "commit": report["commit"],
                "methods": {
                    m: {"ready": r["ready"], "cpu_s": [s["cpu_s"] for s in r["runs"]]}
                    for m, r in report["methods"].items()
                },
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--plot", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.rounds <= 16:
        parser.error("--rounds must be in1..16")
    compare(args.inputs, args.output, args.rounds, args.trace, args.plot)


if __name__ == "__main__":
    main()
