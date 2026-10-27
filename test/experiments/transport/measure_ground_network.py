"""Compare existing common-distance and LP whole-profile cuts on the same source tree."""

import argparse
import json
import os
import platform
import subprocess
import time
from functools import partial
from pathlib import Path

import memray
import numpy as np
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground import fit_ground
from outshine_ref.transport.canonical_ports._ground_network_input import (
    prepare_network_ground,
)
from outshine_ref.transport.canonical_ports._ground_scope_field import affine_surface
from outshine_ref.transport.canonical_ports._source import component, source_index
from replay_branched_ground import INPUT
from replay_canonical_bridge_ports import allocations
from replay_canonical_ports import process_peak, serializable
from replay_ground_network import SEED


def stage(index, selected, terrain, budget, method):
    started = time.process_time()
    wall_started = time.perf_counter()
    _, inputs, _, geometry = prepare_network_ground(
        index,
        selected,
        terrain,
        budget,
        deadline=time.monotonic() + budget.surface.time_limit_s,
        cut_method=method,
    )
    field, report = fit_ground(inputs, 0.5, "auto", budget)
    if field is None or hasattr(field, "plan"):
        raise ValueError(
            "this fixed comparison requires both validated affine controls"
        )
    mesh, _, checked = affine_surface(inputs, field)
    if report["maximum_original_prior_error_m"] > 0.5:
        raise ValueError("same prior limit required")
    return {
        "cpu_ms": 1000 * (time.process_time() - started),
        "wall_ms": 1000 * (time.perf_counter() - wall_started),
        "vertices": len(mesh.vertices),
        "faces": len(mesh.triangles),
        "parts": len(inputs.part_roles),
        "ports": len(inputs.port_edges),
        "terrain_samples": geometry["terrain_samples"],
        "cut_unknowns": sum(len(r["cuts_m"]) for r in geometry["cut_fits"]),
        "cut_constraint_rows": sum(r["constraint_rows"] for r in geometry["cut_fits"]),
        "processed_cut_constraint_rows": sum(
            r["processed_constraint_rows"] for r in geometry["cut_fits"]
        ),
        "linear_cut_solves": sum(r["method"] == "linear" for r in geometry["cut_fits"]),
        "total_cut_distance_m": sum(sum(r["cuts_m"]) for r in geometry["cut_fits"]),
        "input_owned_payload_bytes": sum(
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
        "maximum_original_prior_error_m": report["maximum_original_prior_error_m"],
        "maximum_normalized_finite_grade": checked["maximum_normalized_finite_grade"],
    }


def short_arm_control(index, terrain, budget, unknown):
    selected, closure, _ = component(index, (5699117369358000712, 1), unknown, budget)
    _, inputs, _, geometry = prepare_network_ground(
        index,
        selected,
        terrain,
        budget,
        deadline=time.monotonic() + budget.surface.time_limit_s,
        cut_method="common",
    )
    field, fitting = fit_ground(inputs, 0.5, "auto", budget)
    return {
        "scope": "Common-only geometric control on the existing short three-arm case; not the selected recipe.",
        "field_available": field is not None,
        "original_closure_reason": closure,
        "geometry": geometry,
        "fitting": fitting,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--short-arm-control", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    source, *_ = load_bridge_source_facts(INPUT)
    budget = PortBudget()
    index, reason = source_index(source, budget)
    if reason:
        raise ValueError(reason)
    selected, _, _ = component(index, SEED, set(source.unknown_nodes), budget)
    manifest = json.loads((INPUT / "manifest.json").read_text())
    terrain, terrain_binding = terrain_input(
        FIXTURE, {"terrain": manifest["terrain_reference"]}
    )
    calls = {
        m: partial(stage, index, selected, terrain, budget, m)
        for m in ("adaptive", "linear", "common")
    }
    for call in calls.values():
        call()
    samples = {m: [] for m in calls}
    for order in (
        ("adaptive", "linear", "common"),
        ("common", "linear", "adaptive"),
        ("linear", "adaptive", "common"),
    ):
        for method in order:
            samples[method].append(calls[method]())
    traces = {}
    for method, call in calls.items():
        path = args.output / (method + ".bin")
        with memray.Tracker(str(path), trace_python_allocators=True):
            call()
        traces[method] = allocations(path)
        traces[method]["scope"] = (
            "Warm geometry/DEM/affine-field stage only; decoded/indexed Source and warm imports excluded. Traced Python allocations, not total copied bytes or native process peak."
        )
    counter = (
        short_arm_control(index, terrain, budget, set(source.unknown_nodes))
        if args.short_arm_control
        else None
    )
    peak, origin = process_peak()
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "working_tree_dirty": bool(
            subprocess.check_output(["git", "status", "--porcelain"])
        ),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "source_binding": source.binding,
        "terrain_binding": terrain_binding,
        "seed": SEED,
        "selected_source_spans": selected,
        "short_arm_common_control": counter,
        "methods": samples,
        "allocations": traces,
        "worker_peak_bytes": peak,
        "worker_peak_origin": origin,
        "scope": "Fixed real five-span geometry/DEM/affine-field stage, existing decoded/indexed Source. Three warm alternating single-host CPU samples; tracing separate. Owned payload is not total copied bytes. Common is an experimental unbound control. Both preserve missing Source/contact admission; no World/native/device result.",
    }
    (args.output / "report.json").write_text(
        json.dumps(serializable(report), default=lambda v: v.tolist(), indent=2) + "\n"
    )
    print(
        json.dumps(
            {
                m: {
                    "cpu_ms": [r["cpu_ms"] for r in samples[m]],
                    "work": samples[m][0],
                    "allocations": traces[m],
                }
                for m in calls
            },
            default=lambda v: v.tolist(),
        )
    )


if __name__ == "__main__":
    main()
