"""Equal-output eager versus shared indoor stair/plate composition (#1558)."""

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
from compare_indoor_floors import DATA, allocations, json_value
from indoor_stair_batch import compose_stairs
from indoor_stair_contacts import group_stair_aliases
from indoor_stair_inputs import load_stair_context
from indoor_stair_recipe import StairLimits


def signature(result):
    value = hashlib.sha256(
        json.dumps(result["source_ledger"], sort_keys=True, default=json_value).encode()
    )
    for stair in result["stairs"]:
        value.update(stair["source_id"].encode())
        value.update(json.dumps(stair["source_ids"]).encode())
        for name in ("vertices", "faces", "parts"):
            value.update(stair["mesh"][name].tobytes())
    for floor in result["floors"]:
        value.update(floor.identifier.encode())
        value.update(floor.polygon.wkb)
    for key, mesh in sorted(result["modified_floor_meshes"].items()):
        value.update(key.encode())
        for name in ("vertices", "faces", "parts"):
            value.update(mesh[name].tobytes())
    for r in result["reservations"]:
        value.update(r.owner_id.encode())
        value.update(r.geometry.wkb)
        value.update(np.array([r.heights.lower_m, r.heights.upper_m]).tobytes())
    return value.hexdigest()


def describe(result):
    return {
        "work": result["work"],
        "source_ledger": result["source_ledger"],
        "source_status_counts": dict(
            Counter(r["status"] for r in result["source_ledger"])
        ),
        "pending_reasons": dict(
            Counter(
                r["reason"] for r in result["source_ledger"] if r["status"] == "pending"
            )
        ),
        "stair_payload_bytes": sum(
            s["mesh_audit"]["payload_bytes"] for s in result["stairs"]
        ),
        "modified_plate_payload_bytes": sum(
            o["mesh_audit"]["payload_bytes"] for o in result["openings"]
        ),
        "stairs": [
            {
                k: s[k]
                for k in (
                    "source_id",
                    "source_ids",
                    "recipe",
                    "mesh_audit",
                    "source_audit",
                    "end_profiles",
                    "final_port_tests",
                    "qualification",
                )
            }
            for s in result["stairs"]
        ],
        "openings": result["openings"],
        "reservations": [
            {
                "owner_id": r.owner_id,
                "purpose": r.purpose,
                "wkb_hex": r.geometry.wkb_hex,
                "height_interval_m": asdict(r.heights),
            }
            for r in result["reservations"]
        ],
        "source_features": [
            {
                "source_id": s.source_id,
                "wkb_hex": s.geometry.wkb_hex,
                "attributes": dict(s.attributes),
            }
            for s in result["source_features"]
        ],
        "artifact_sha256": signature(result),
    }


def compare(args):
    args.output.mkdir(parents=True, exist_ok=False)
    context = load_stair_context(args.inputs)
    limits = StairLimits()
    receipts = {r["sha256"]: r for r in context.inputs.metadata["source_receipts"]}
    cpu, wall = time.process_time(), time.perf_counter()
    groups = group_stair_aliases(context.bindings, receipts, limits)
    group_cost = {
        "cpu_s": time.process_time() - cpu,
        "wall_s": time.perf_counter() - wall,
    }

    def generate(method):
        return compose_stairs(
            groups, context.floors, context.requests, limits=limits, method=method
        )

    results = {m: generate(m) for m in ("eager", "shared")}
    expected = signature(results["shared"])
    if not results["shared"]["stairs"]:
        raise ValueError("No jointly qualified body")
    samples = {m: [] for m in results}
    for turn in range(args.rounds):
        for method in tuple(results) if turn % 2 == 0 else tuple(reversed(results)):
            cpu, wall = time.process_time(), time.perf_counter()
            value = generate(method)
            samples[method].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            if signature(value) != expected:
                raise ValueError("Joint geometry/source ledger/reservations differ")
    loop_rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * (
        1 if platform.system() == "Darwin" else 1024
    )
    traces = {}
    if args.trace:
        for method in results:
            value, traces[method] = allocations(
                args.output / f"{method}.bin", lambda method=method: generate(method)
            )
            if signature(value) != expected:
                raise ValueError("Profiled joint product differs")
    result = results["shared"]
    arrays = {}
    floor_models = []
    for index, stair in enumerate(result["stairs"]):
        arrays.update({f"stair/{index}/{k}": a for k, a in stair["mesh"].items()})
    for index, floor in enumerate(result["floors"]):
        mesh = result["modified_floor_meshes"].get(
            floor.identifier, context.floors["meshes"][floor.identifier]
        )
        arrays.update({f"floor/{index}/{k}": a for k, a in mesh.items()})
        floor_models.append(
            {
                "npz_index": index,
                "id": floor.identifier,
                "source_ids": floor.source_ids,
                "level": floor.level,
                "lower_z_m": floor.lower_z_m,
                "upper_z_m": floor.upper_z_m,
                "wkb_hex": floor.polygon.wkb_hex,
            }
        )
    np.savez(args.output / "meshes.npz", **arrays)
    report = {
        "schema": "outshine/indoor-stair-batch-comparison/1",
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "worktree_status": subprocess.check_output(
            ["git", "status", "--porcelain"], text=True
        ),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "load_average": os.getloadavg(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "threads": {
            k: os.environ.get(k) for k in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
        "input": context.inputs.metadata,
        "origin_lat_lon": context.inputs.origin,
        "frame": result["frame"].identifier,
        "limits": asdict(limits),
        "batch_limits": {"max_bodies": 16, "max_groups": 64, "headroom_m": 2.1},
        "source_cost": context.source_cost,
        "expanded_floor_context_cost": context.floor_cost,
        "alias_group_cost": group_cost,
        "original_request_responses": context.original_responses,
        "old_ready_xyz_exact": context.old_ready_count,
        "existing_floor_choices": context.floors["choices"],
        "final_floor_models": floor_models,
        "generation_loop_process_peak_rss_bytes": loop_rss,
        "methods": {
            m: describe(r) | {"runs": samples[m], "allocations": traces.get(m)}
            for m, r in results.items()
        },
        "scope": result["qualification"]
        + " Timed bounded composition includes rejected candidates, actual body/plate construction and checks. Raw-source/DEM/ENU, expanded floors and alias preparation are separate. Profile/plot/NPZ work is excluded. Allocation requests, scoped peak, process RSS and payload sizes differ; total copied bytes, internet acquisition, finished asset-cache hits and device/frame-p99 are not measured.",
    }
    (args.output / "report.json").write_text(
        json.dumps(report, indent=2, default=json_value) + "\n"
    )
    if args.plot:
        from indoor_stair_batch_plot import draw

        draw(context, result, args.output)
    print(
        json.dumps(
            {
                "output": str(args.output),
                "commit": report["commit"],
                "methods": {
                    m: {
                        "source_status_counts": r["source_status_counts"],
                        "work": r["work"],
                        "cpu_s": [v["cpu_s"] for v in r["runs"]],
                    }
                    for m, r in report["methods"].items()
                },
            },
            default=json_value,
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
        parser.error("--rounds must be1..16")
    compare(args)


if __name__ == "__main__":
    main()
