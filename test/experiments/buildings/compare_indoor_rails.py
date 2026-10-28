"""Equal-geometry independent support meshes versus affine templates (#1565)."""

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
from compare_indoor_stair_batch import signature as stair_signature
from indoor_mesh_checks import closed_mesh
from indoor_rail_checks import qualify_rails
from indoor_rail_instances import materialize
from indoor_rails import compose_rails
from indoor_stair_batch import compose_stairs
from indoor_stair_contacts import group_stair_aliases
from indoor_stair_inputs import load_stair_context
from indoor_stair_recipe import StairLimits


def signature(result):
    digest = hashlib.sha256(json.dumps(result["ledger"], sort_keys=True).encode())
    for row in (*result["handrails"], *result["guards"]):
        for name in ("vertices", "faces", "parts"):
            digest.update(row["mesh"][name].tobytes())
    for row in result["supports"]:
        digest.update(row["source_id"].encode())
        digest.update(row["kind"].encode())
        digest.update(row["transform"].tobytes())
    return digest.hexdigest()


def equivalent(eager, shared):
    if signature(eager) != signature(shared):
        raise ValueError("Rail paths, meshes, support poses or source ownership differ")
    maximum, bound, volume_error = 0.0, 0.0, 0.0
    for a, b in zip(eager["supports"], shared["supports"], strict=True):
        left = a["mesh"]
        right = materialize(shared["templates"][b["kind"]], b["transform"])
        distances = np.linalg.norm(
            left["vertices"][:, None] - right["vertices"][None], axis=2
        )
        delta = max(distances.min(axis=0).max(), distances.min(axis=1).max())
        arithmetic = 128 * np.spacing(max(1.0, float(abs(left["vertices"]).max())))
        if delta > arithmetic:
            raise ValueError(
                "Independent support geometry differs beyond arithmetic bound"
            )
        aa, bb = closed_mesh(left), closed_mesh(right)
        error = abs(aa["signed_volume_m3"] - bb["signed_volume_m3"])
        if error > arithmetic * max(aa["surface_area_m2"], bb["surface_area_m2"]):
            raise ValueError("Independent support material volumes differ")
        maximum, bound, volume_error = (
            max(maximum, float(delta)),
            max(bound, float(arithmetic)),
            max(volume_error, error),
        )
    return {
        "support_pairs": len(eager["supports"]),
        "maximum_vertex_set_distance_m": maximum,
        "maximum_arithmetic_bound_m": bound,
        "maximum_material_volume_difference_m3": volume_error,
        "scope": "Bidirectional actual-vertex-set distance and closed positive material volume per simple prism/sweep, with complete traffic/source outer-envelope checks on both outputs. Different triangulations/arithmetic are permitted; not byte identity or a generic mesh-equivalence oracle.",
    }


def prepare(inputs):
    context = load_stair_context(inputs)
    receipts = {r["sha256"]: r for r in context.inputs.metadata["source_receipts"]}
    groups = group_stair_aliases(context.bindings, receipts, StairLimits())
    cpu, wall = time.process_time(), time.perf_counter()
    batch = compose_stairs(groups, context.floors, context.requests)
    return (
        context,
        batch,
        {"cpu_s": time.process_time() - cpu, "wall_s": time.perf_counter() - wall},
    )


def compare(args):
    args.output.mkdir(parents=True, exist_ok=False)
    context, batch, batch_cost = prepare(args.inputs)
    baseline = stair_signature(batch)

    def generate(method):
        return compose_rails(batch, context.floors["reservations"], method=method)

    results = {m: generate(m) for m in ("eager", "shared")}
    expected = signature(results["shared"])
    if not results["shared"]["handrails"]:
        raise ValueError("No actual whole railing candidate")
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
                raise ValueError("Railing candidates changed between runs")
    peak_rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * (
        1 if platform.system() == "Darwin" else 1024
    )
    traces = {}
    if args.trace:
        for method in results:
            value, traces[method] = allocations(
                args.output / f"{method}.bin", lambda method=method: generate(method)
            )
            if signature(value) != expected:
                raise ValueError("Profiled railing output changed")
    qualification = {
        m: qualify_rails(r, batch, context.floors) for m, r in results.items()
    }
    agreement = equivalent(results["eager"], results["shared"])
    if stair_signature(batch) != baseline:
        raise ValueError("Base stairs, plates, ports or source ledger changed")
    result = results["shared"]
    arrays = {}
    for kind in ("handrails", "guards"):
        for index, row in enumerate(result[kind]):
            arrays.update({f"{kind}/{index}/{k}": a for k, a in row["mesh"].items()})
    for kind, mesh in result["templates"].items():
        arrays.update({f"templates/{kind}/{k}": a for k, a in mesh.items()})
    for index, row in enumerate(result["supports"]):
        arrays[f"supports/{index}/transform"] = row["transform"]
    for index, stair in enumerate(batch["stairs"]):
        arrays.update({f"stairs/{index}/{k}": a for k, a in stair["mesh"].items()})
    np.savez(args.output / "meshes.npz", **arrays)
    report = {
        "schema": "outshine/indoor-rail-comparison/1",
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
        "frame": batch["frame"].identifier,
        "source_cost": context.source_cost,
        "expanded_floor_cost": context.floor_cost,
        "base_stair_batch_cost": batch_cost,
        "base_stair_signature": baseline,
        "original_request_responses": context.original_responses,
        "old_ready_xyz_exact": context.old_ready_count,
        "base_source_ledger": batch["source_ledger"],
        "old_floor_choices": context.floors["choices"],
        "base_whole_ports": sum(s["final_port_tests"] for s in batch["stairs"]),
        "limits": asdict(result["limits"]),
        "support_limits": asdict(result["support_limits"]),
        "ledger": result["ledger"],
        "qualified_local_source_owners": [
            r["source_id"] for r in result["ledger"] if r["status"] == "candidate"
        ],
        "opening_ledger": result["opening_ledger"],
        "support_kinds": dict(Counter(s["kind"] for s in result["supports"])),
        "support_models": [
            {k: v for k, v in s.items() if k != "mesh"} for s in result["supports"]
        ],
        "agreement": agreement,
        "generation_loop_process_peak_rss_bytes": peak_rss,
        "methods": {
            m: {
                "work": r["work"],
                "runs": samples[m],
                "allocations": traces.get(m),
                "actual_output_qualification": qualification[m],
            }
            for m, r in results.items()
        },
        "scope": result["scope"]
        + " Candidate generation includes rail meshes, support planning, template or full support construction and local construction checks. The identical independent full-output envelope/mesh qualification, source preparation, NPZ, plots and profiler processing are excluded from generation timing. Total copied bytes, native cache/upload/device times and frame p99 remain unmeasured. Hollow support sidewalls are2mm; sealed end caps scale with height at the explicit0.002fraction. Open bar/post geometry is no infill, child-safety or structural certification.",
    }
    (args.output / "report.json").write_text(
        json.dumps(report, indent=2, default=json_value) + "\n"
    )
    if args.plot:
        from indoor_rail_plot import draw

        draw(batch, result, args.output)
    print(
        json.dumps(
            {
                "output": str(args.output),
                "agreement": agreement,
                "methods": {
                    m: {"work": r["work"], "cpu_s": [v["cpu_s"] for v in samples[m]]}
                    for m, r in results.items()
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
