"""One declared real stair batch, with unchanged floor contacts and scoped costs."""

import argparse
import hashlib
import json
import os
import platform
import resource
import subprocess
import time
from dataclasses import asdict
from pathlib import Path

import numpy as np
from compare_indoor_floors import DATA, allocations, json_value
from indoor_floor_geometry import produce_floor_contacts
from indoor_floor_inputs import load_inputs
from indoor_floor_plan import FloorLimits
from indoor_stair_contacts import bind_stairs
from indoor_stair_recipe import StairLimits
from indoor_stair_site import build_stair


def signature(result):
    digest = hashlib.sha256()
    digest.update(json.dumps(result["recipe"], sort_keys=True).encode())
    for key, mesh in [
        ("stair", result["mesh"]),
        *sorted(result["modified_floor_meshes"].items()),
    ]:
        digest.update(key.encode())
        for name in ("vertices", "faces", "parts"):
            digest.update(mesh[name].tobytes())
    for floor in result["floors"]:
        digest.update(floor.polygon.wkb)
    digest.update(result["frame"].identifier.encode())
    for reservation in result["reservations"]:
        digest.update(reservation.geometry.wkb)
        digest.update(
            np.array(
                [reservation.heights.lower_m, reservation.heights.upper_m]
            ).tobytes()
        )
    return digest.hexdigest()


def compare(args):
    args.output.mkdir(parents=True, exist_ok=False)
    cpu, wall = time.process_time(), time.perf_counter()
    inputs = load_inputs(args.inputs)
    source_cost = {
        "cpu_s": time.process_time() - cpu,
        "wall_s": time.perf_counter() - wall,
    }
    cpu, wall = time.process_time(), time.perf_counter()
    original = {r.identifier: r for r in inputs.requests}
    requests = tuple(
        original.get(r.identifier, r)
        for r in inputs.constraints
        if r.purpose == "flat_indoor_walkway"
    )
    floors = produce_floor_contacts(
        inputs.parts,
        requests,
        inputs.ground,
        method="adapted",
        constraints=inputs.constraints,
        limits=FloorLimits(max_requests=128),
    )
    context_cost = {
        "cpu_s": time.process_time() - cpu,
        "wall_s": time.perf_counter() - wall,
    }
    old = produce_floor_contacts(
        inputs.parts,
        inputs.requests,
        inputs.ground,
        method="adapted",
        constraints=inputs.constraints,
    )
    choices = {r["source_id"]: r for r in floors["choices"]}
    old_ready = [r for r in old["choices"] if r["status"] == "ready"]
    for row in old_ready:
        np.testing.assert_array_equal(row["xyz"], choices[row["source_id"]]["xyz"])
    bindings = bind_stairs(inputs, floors)
    binding = next((b for b in bindings if b["source_id"] == args.source), None)
    if binding is None:
        raise ValueError("Requested source is not in the declared raw stair snapshot")
    limits = StairLimits()

    def generate(method):
        return build_stair(
            binding, floors, requests, limits=limits, source_method=method
        )

    results = {m: generate(m) for m in ("repeated", "bands")}
    if any(r["status"] != "ready" for r in results.values()):
        raise ValueError({m: r["reason"] for m, r in results.items()})
    expected = signature(results["bands"])
    samples = {m: [] for m in results}
    for turn in range(args.rounds):
        for method in tuple(results) if turn % 2 == 0 else tuple(reversed(results)):
            cpu, wall = time.process_time(), time.perf_counter()
            result = generate(method)
            samples[method].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            if signature(result) != expected:
                raise ValueError(
                    "Stair/floor geometry changed across equivalent proofs or runs"
                )
    loop_rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * (
        1 if platform.system() == "Darwin" else 1024
    )
    profiles = {}
    if args.trace:
        for method in results:
            result, profiles[method] = allocations(
                args.output / f"{method}.bin", lambda method=method: generate(method)
            )
            if signature(result) != expected:
                raise ValueError("Profiled geometry differs")
    result = results["bands"]
    final = {f.identifier: f for f in result["floors"]}
    for request in requests:
        row = choices[request.identifier]
        if row["status"] == "ready" and not final[row["floor_id"]].polygon.covers(
            request.corridor
        ):
            raise ValueError("Opening invalidates an existing chosen corridor")
    arrays = {"stair/" + k: a for k, a in result["mesh"].items()}
    floor_index = []
    for index, floor in enumerate(result["floors"]):
        mesh = result["modified_floor_meshes"].get(
            floor.identifier, floors["meshes"][floor.identifier]
        )
        arrays.update({f"floor/{index}/{k}": a for k, a in mesh.items()})
        floor_index.append(
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
    candidates = []
    for b in bindings:
        r = build_stair(b, floors, requests)
        candidates.append(
            {
                k: b[k]
                for k in ("source_id", "original", "frame", "ends", "qualification")
            }
            | {
                "candidate_status": r["status"],
                "candidate_reason": r["reason"],
                "emitted": b["source_id"] == args.source,
                "omission_reason": ""
                if b["source_id"] == args.source
                else r["reason"] or "outside_requested_stair_batch",
            }
        )
    report = {
        "schema": "outshine/indoor-stair-comparison/1",
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
        "input": inputs.metadata,
        "limits": asdict(limits),
        "source_cost": source_cost,
        "expanded_floor_context_cost": context_cost,
        "source_id": binding["source_id"],
        "ends": binding["ends"],
        "recipe": result["recipe"],
        "search": result["search"],
        "mesh_audit": result["mesh_audit"],
        "frame": result["frame"].identifier,
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
                "source_id": r.source_id,
                "wkb_hex": r.geometry.wkb_hex,
                "attributes": dict(r.attributes),
            }
            for r in result["source_features"]
        ],
        "openings": result["openings"],
        "final_floor_models": floor_index,
        "existing_floor_choices": floors["choices"],
        "original_request_responses": old["choices"],
        "old_ready_xyz_exact": len(old_ready),
        "expanded_ready_corridors_preserved": sum(
            r["status"] == "ready" for r in floors["choices"]
        ),
        "source_candidate_checks": candidates,
        "candidate_scope": "Each raw stair tested independently against the same chosen floor context; only the explicitly requested source is emitted. Candidate successes are not a jointly collision-qualified stair batch. Original OSM node identity and native Source/World admission remain absent.",
        "generation_loop_process_peak_rss_bytes": loop_rss,
        "artifact_sha256": expected,
        "methods": {
            m: {
                "runs": samples[m],
                "allocations": profiles.get(m),
                "source_audit": r["source_audit"],
            }
            for m, r in results.items()
        },
        "greedy": {
            k: v
            for k, v in build_stair(binding, floors, requests, method="greedy").items()
            if k in ("status", "reason", "search", "recipe")
        },
        "scope": "Closed chosen stair, whole landings, emitted plate openings and exact union-envelope stratum coverage with recipe roof reserve. No Source-envelope internal plane is automatically a solid beam. Actual floor slabs stay hard obstacles. No whole-terrain/statics, walls/doors/guardrails/public-route, original OSM topology/native World or device/frame qualification. Source acquisition/asset-cache hits and total copied bytes are not measured; payloads, allocation traffic, scoped peak and RSS differ.",
    }
    (args.output / "report.json").write_text(
        json.dumps(report, indent=2, default=json_value) + "\n"
    )
    if args.plot:
        from indoor_stair_plot import draw

        draw(binding, floors, result, args.output)
    print(
        json.dumps(
            {
                "output": str(args.output),
                "source": binding["source_id"],
                "recipe": result["recipe"],
                "methods": report["methods"],
            },
            default=json_value,
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=DATA)
    parser.add_argument("--source", required=True)
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
