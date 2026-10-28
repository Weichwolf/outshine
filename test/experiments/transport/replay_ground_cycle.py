"""Reproduce and compare bounded cuts on one whole actual source cycle (#1569)."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from ground_cycle import CYCLE_RECIPE, generate
from ground_cycle_checks import audit
from ground_network_plot import plot
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground_recipe import NETWORK_RECIPE
from outshine_ref.transport.canonical_ports._ground_scope import (
    prepare_ground_field_scope,
)
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)
from outshine_ref.transport.canonical_ports._source import component
from replay_branched_ground import INPUT
from replay_canonical_bridge_ports import allocations
from replay_canonical_ports import process_peak, serializable

SEED = (11372695559320524359, 0)


def prepare(bundle=None):
    started = time.process_time()
    source, groups, ways, binding, reader = (
        load_bridge_source_facts(INPUT) if bundle is None else bundle
    )
    budget = PortBudget()
    index, reason, work = prepare_ground_scope_index(
        source,
        groups,
        ways,
        source_binding=source.binding,
        candidate_binding=binding,
        budget=budget,
    )
    if reason:
        raise ValueError(reason)
    selected, closure, closure_work = component(
        index.index, SEED, index.unknown, budget
    )
    if closure not in ("", "unknown_source_frontier"):
        raise ValueError(closure)
    _, _, _, shared_reason, shared_work = prepare_ground_field_scope(
        source,
        SEED,
        groups,
        ways,
        source_binding=source.binding,
        candidate_binding=binding,
        scope_index=index,
        geometry_recipe=NETWORK_RECIPE,
        budget=budget,
    )
    manifest = json.loads((INPUT / "manifest.json").read_text())
    terrain, terrain_binding = terrain_input(
        FIXTURE, {"terrain": manifest["terrain_reference"]}
    )
    metadata = {
        "source_binding": source.binding,
        "candidate_binding": binding,
        "terrain_binding": terrain_binding,
        "reader": reader,
        "manifest_sha256": hashlib.sha256(
            (INPUT / "manifest.json").read_bytes()
        ).hexdigest(),
        "closure_reason": closure,
        "closure_work": closure_work,
        "strict_existing_tree_entry_reason": shared_reason,
        "shared_scope_work": shared_work,
        "source_index_work": work,
        "original_unknown_nodes": source.unknown_nodes,
        "qualified_world_joins": 0,
        "qualified_original_end_contacts": 0,
        "source_preparation_cpu_s": time.process_time() - started,
        "source_admission": "Pending: no registered canonical cycle/source contract; original Unknown and native fact omissions remain unchanged.",
    }
    return index.index, selected, terrain, budget, metadata


def describe(result, checked):
    geometry = result["geometry"]
    inputs, mesh = result["inputs"], result["mesh"]
    return {
        "geometry": geometry,
        "fitting": result["fitting"],
        "quality": result["quality"],
        "independent_audit": checked,
        "source_members": result["source_members"],
        "source_terminal_cuts": result["source_terminal_cuts"],
        "linear_cut_solves": sum(r["method"] == "linear" for r in geometry["cut_fits"]),
        "processed_cut_constraint_rows": sum(
            r["processed_constraint_rows"] for r in geometry["cut_fits"]
        ),
        "known_plan_copy_bytes": result["known_plan_copy_bytes"],
        "input_payload_bytes": sum(
            a.nbytes
            for a in (
                inputs.surface.vertices,
                inputs.surface.triangles,
                inputs.face_parts,
                inputs.part_roles,
                inputs.port_edges,
                inputs.port_parts,
                inputs.grade_frames,
            )
        ),
        "output_payload_bytes": mesh.vertices.nbytes + mesh.triangles.nbytes,
    }


def replay(args):
    args.output.mkdir(parents=True, exist_ok=False)
    index, selected, terrain, budget, metadata = prepare()

    def stage(method):
        cpu, wall = time.process_time(), time.perf_counter()
        result = generate(index, selected, terrain, budget, cut_method=method)
        checked = audit(result)
        return (
            result,
            checked,
            {"cpu_s": time.process_time() - cpu, "wall_s": time.perf_counter() - wall},
        )

    results = {m: stage(m) for m in ("adaptive", "linear")}
    samples = {m: [] for m in results}
    for turn in range(args.rounds):
        for method in tuple(results) if turn % 2 == 0 else tuple(reversed(results)):
            result, checked, cost = stage(method)
            if checked != results[method][1]:
                raise ValueError(
                    "Cycle whole-output qualification changed between runs"
                )
            np.testing.assert_array_equal(
                result["mesh"].vertices, results[method][0]["mesh"].vertices
            )
            np.testing.assert_array_equal(
                result["mesh"].triangles, results[method][0]["mesh"].triangles
            )
            samples[method].append(cost)
    peak, peak_source = process_peak()
    traces = {}
    if args.trace:
        import memray

        for method in results:
            path = args.output / f"{method}.bin"
            with memray.Tracker(
                str(path), trace_python_allocators=True, native_traces=True
            ):
                stage(method)
            traces[method] = allocations(path)
            traces[method]["scope"] = (
                "Warm geometry/DEM/field plus independent whole-output checks, including traced Python and native allocations. Source preparation and profiler processing excluded; requested bytes are not copied bytes."
            )
    arrays = {}
    for method, (result, _, _) in results.items():
        mesh, inputs = result["mesh"], result["inputs"]
        arrays.update(
            {
                f"{method}/vertices": mesh.vertices,
                f"{method}/triangles": mesh.triangles,
                f"{method}/prior_vertices": inputs.surface.vertices,
                f"{method}/prior_triangles": inputs.surface.triangles,
                f"{method}/port_edges": inputs.port_edges,
                f"{method}/port_parts": inputs.port_parts,
            }
        )
    np.savez(args.output / "geometry.npz", **arrays)
    report = {
        "schema": "outshine/ground-cycle-comparison/1",
        "recipe": CYCLE_RECIPE,
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
        "inputs": metadata,
        "seed": SEED,
        "maximum_prior_change_m": 0.5,
        "methods_have_identical_mesh_buffers": all(
            np.array_equal(
                results["adaptive"][0]["mesh"].vertices, r[0]["mesh"].vertices
            )
            and np.array_equal(
                results["adaptive"][0]["mesh"].triangles, r[0]["mesh"].triangles
            )
            for r in results.values()
        ),
        "process_peak_rss_bytes_before_profile_plot": peak,
        "process_peak_source": peak_source,
        "methods": {
            m: describe(r, q) | {"runs": samples[m], "allocations": traces.get(m)}
            for m, (r, q, _) in results.items()
        },
        "scope": "Same whole source cycle, widths/grades and0.5m prior limit; cut procedures may produce different domains/vertices and no common global optimum is claimed. Timing includes geometry, DEM, fit and independent actual-output audit; source decode/index, tracing/trace processing, NPZ and plots are separate. Known plan copy bytes and retained payload do not establish total copied bytes/app/device memory. Source/terminal/World/bridge/vehicle/device/p99 remain unqualified.",
    }
    (args.output / "report.json").write_text(
        json.dumps(serializable(report), indent=2, default=lambda value: value.tolist())
        + "\n"
    )
    if args.plot:
        result = results["adaptive"][0]
        result["surface_source_faces"] = (
            result["field"].plan.source_faces
            if hasattr(result["field"], "plan")
            else np.arange(len(result["mesh"].triangles))
        )
        plot(
            args.output,
            result["inputs"],
            result["mesh"],
            result,
            title="Six actual source spans / one preserved loop / twelve whole internal profiles; Source admission Pending",
        )
    print(
        json.dumps(
            {
                "output": str(args.output),
                "source_admission": metadata["source_admission"],
                "methods": {
                    m: {
                        "quality": q,
                        "cpu_s": [s["cpu_s"] for s in samples[m]],
                        "linear_cut_solves": describe(r, q)["linear_cut_solves"],
                    }
                    for m, (r, q, _) in results.items()
                },
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
        parser.error("--rounds must be1..16")
    replay(args)


if __name__ == "__main__":
    main()
