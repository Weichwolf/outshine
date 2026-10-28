"""Same-input adaptive/all-node-LP comparison for actual whole two-loop components."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from ground_two_cycles import generate
from ground_two_cycles_checks import audit
from ground_two_cycles_inputs import prepare
from replay_canonical_bridge_ports import allocations
from replay_canonical_ports import process_peak, serializable
from replay_ground_cycle import describe


def stage(index, cases, terrain, budget, method):
    started, tick = time.process_time(), time.perf_counter()
    records = {}
    for case in cases:
        try:
            result = generate(
                index, case["selected"], terrain, budget, cut_method=method
            )
            checked = audit(result)
        except ValueError as error:
            records[case["key"]] = {
                "state": "PendingOriginalRetained",
                "reason": str(error),
            }
        else:
            records[case["key"]] = {
                "state": "FiniteGeometryOnly",
                "result": result,
                "audit": checked,
            }
    return records, {
        "cpu_s": time.process_time() - started,
        "wall_s": time.perf_counter() - tick,
    }


def fingerprint(records):
    digest = hashlib.sha256()
    for key, row in sorted(records.items()):
        digest.update(key.encode())
        digest.update(row["state"].encode())
        if "result" in row:
            mesh = row["result"]["mesh"]
            digest.update(mesh.vertices.tobytes())
            digest.update(mesh.triangles.tobytes())
        else:
            digest.update(row["reason"].encode())
    return digest.hexdigest()


def export(output, results):
    arrays, records = {}, {}
    for method, rows in results.items():
        records[method] = {}
        for key, row in rows.items():
            if "result" not in row:
                records[method][key] = row
                continue
            result = row["result"]
            mesh, inputs = result["mesh"], result["inputs"]
            prefix = method + "_" + key + "_"
            for name, value in {
                "vertices": mesh.vertices,
                "faces": mesh.triangles,
                "prior_vertices": inputs.surface.vertices,
                "prior_faces": inputs.surface.triangles,
                "port_edges": inputs.port_edges,
                "port_parts": inputs.port_parts,
                "grade_frames": inputs.grade_frames,
                "face_parts": result["face_parts"],
            }.items():
                arrays[prefix + name] = value
            records[method][key] = {
                "state": row["state"],
                "array_prefix": prefix,
            } | describe(result, row["audit"])
    np.savez_compressed(output / "geometry.npz", **arrays)
    return records


def replay(args):
    dirty = subprocess.check_output(["git", "status", "--porcelain"], text=True)
    if dirty:
        raise ValueError("Freeze complete two-cycle source before evidence")
    args.output.mkdir(parents=True, exist_ok=False)
    index, cases, terrain, budget, metadata = prepare()
    methods = ("adaptive", "linear")
    results = {
        method: stage(index, cases, terrain, budget, method)[0] for method in methods
    }
    hashes = {method: fingerprint(records) for method, records in results.items()}
    runs, traces = {method: [] for method in methods}, {}
    for turn in range(args.rounds):
        for method in methods if turn % 2 == 0 else reversed(methods):
            records, cost = stage(index, cases, terrain, budget, method)
            if fingerprint(records) != hashes[method]:
                raise ValueError(
                    "Complete two-cycle output changed during timed replay"
                )
            runs[method].append(cost)
    if args.trace:
        import memray

        for method in methods:
            path = args.output / (method + ".bin")
            with memray.Tracker(
                str(path), trace_python_allocators=True, native_traces=True
            ):
                records, _ = stage(index, cases, terrain, budget, method)
            if fingerprint(records) != hashes[method]:
                raise ValueError("Profiled two-cycle output changed")
            traces[method] = allocations(path) | {
                "scope": "Complete five selected original components: topology guards, layout, original-hole control, DEM, height fit, whole quality, independent mesh audit and retained output. Index/acquisition, trace processing, export and images excluded. Requested bytes are not copied bytes."
            }
    records = export(args.output, results)
    peak, peak_source = process_peak()
    report = {
        "schema": "outshine/actual-two-cycle-ground-comparison/1",
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "worktree_status": dirty,
        "platform": platform.platform(),
        "python": platform.python_version(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "load_average": os.getloadavg(),
        "threads": {
            key: os.environ.get(key)
            for key in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
        "inputs": metadata,
        "original_cases": cases,
        "methods": {
            method: {
                "artifact_sha256": hashes[method],
                "runs": runs[method],
                "allocations": traces.get(method),
                "cases": records[method],
            }
            for method in methods
        },
        "method_mesh_arrays_equal": {
            key: all(
                np.array_equal(
                    getattr(row["result"]["mesh"], field),
                    getattr(results["linear"][key]["result"]["mesh"], field),
                )
                for field in ("vertices", "triangles")
            )
            for key, row in results["adaptive"].items()
            if "result" in row and "result" in results["linear"][key]
        },
        "process_peak_rss_bytes_before_plot": peak,
        "process_peak_source": peak_source,
        "scope": "Finite Python experimental geometry only. Original Unknown/MissingTopology, profile and duplicate-span refusals retained. Whole C1/prior/finite-grade bounds are not dense-DEM, canonical Source/World/native/device/frame-p99 or 8GB acceptance. Known plan copies reported per product, total copied bytes unmeasured.",
    }
    (args.output / "report.json").write_text(
        json.dumps(serializable(report), indent=2) + "\n"
    )
    if args.plot:
        from ground_two_cycles_plot import draw

        draw(args.output, results["adaptive"])
    print(
        json.dumps(
            {
                method: {
                    "whole": sum("result" in r for r in rows.values()),
                    "pending": sum("result" not in r for r in rows.values()),
                }
                for method, rows in results.items()
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--plot", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.rounds <= 16:
        parser.error("Bounded rounds1..16 required")
    replay(args)


if __name__ == "__main__":
    main()
