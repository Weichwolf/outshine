"""Reproduce the shared source-bound three-arm Ground recipe and finite queries."""

import argparse
import json
import subprocess
import time
from contextlib import nullcontext
from pathlib import Path

import numpy as np
from branched_ground_terminals import audit_sections
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from outshine_ref.transport.canonical_ports._ground_recipe import THREE_ARM_RECIPE
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)
from outshine_ref.transport.canonical_ports._ground_terminals import (
    derive_scoped_ground_terminals,
)
from replay_branched_ground import INPUT, ROOT, SEED, plot
from replay_canonical_bridge_ports import allocations
from replay_canonical_ports import serializable


def main(args):
    args.output.mkdir(parents=True, exist_ok=False)
    source, groups, ways, binding, reader = load_bridge_source_facts(INPUT)
    manifest = json.loads((INPUT / "manifest.json").read_text())
    terrain, terrain_binding = terrain_input(
        FIXTURE, {"terrain": manifest["terrain_reference"]}
    )
    began = time.perf_counter()
    index, reason, index_work = prepare_ground_scope_index(
        source, groups, ways, source_binding=source.binding, candidate_binding=binding
    )
    if reason:
        raise ValueError(reason)
    index_seconds = time.perf_counter() - began
    tracker = nullcontext()
    if args.allocation_trace:
        import memray

        tracker = memray.Tracker(
            str(args.allocation_trace), trace_python_allocators=True
        )
    began = time.perf_counter()
    with tracker:
        prepared, reason, report = derive_scoped_ground_terminals(
            source,
            SEED,
            groups,
            ways,
            terrain,
            source_binding=source.binding,
            candidate_binding=binding,
            terrain_binding=terrain_binding,
            maximum_prior_change_m=args.prior_limit,
            geometry_recipe=THREE_ARM_RECIPE,
            method=args.method,
            scope_index=index,
        )
    report.update(
        elapsed_ms=1000 * (time.perf_counter() - began),
        reason=reason,
        commit=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        working_tree_dirty=bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=ROOT, text=True
            )
        ),
        reader_work=reader,
        index_build_work=index_work,
        index_build_ms=1000 * index_seconds,
        platform="shared Linux/Python host, one diagnostic call; no native/device proof",
    )
    if args.allocation_trace:
        report["allocations"] = allocations(args.allocation_trace)
        report["allocations"]["scope"] = (
            "Shared derive_scoped_ground_terminals with decoded inputs/prepared index; reader/index construction and driver excluded. Allocated bytes are not copied bytes or a process peak."
        )
    if prepared is not None:
        mesh = report["surface"]
        report["independent_sections"] = audit_sections(
            prepared[0], mesh, report["terminal_values"]
        )
        np.savez_compressed(
            args.output / "finite.npz",
            vertices=mesh.vertices,
            faces=mesh.triangles,
            parts=prepared[1].plan.face_parts,
            source_faces=prepared[1].plan.source_faces,
            original_vertex_map=prepared[1].plan.original_vertex_map,
            coefficients=prepared[1].plan.plane_coefficients,
            datum=prepared[1].plan.datum_m,
        )
        if args.plot:
            plot(
                args.output,
                report["source_input"],
                mesh,
                {
                    "geometry": report["geometry"],
                    "terminals": {"values": report["terminal_values"]},
                },
                title="Source-qualified three-arm Ground field; no Bridge/World admission",
            )
    payload = {
        k: v
        for k, v in report.items()
        if k
        not in (
            "surface",
            "source_input",
            "source_prior_surface",
            "source_grade_frames",
            "surface_source_faces",
        )
    }
    (args.output / "report.json").write_text(
        json.dumps(serializable(payload), indent=2) + "\n"
    )
    print(
        json.dumps(
            {
                "state": report["state"],
                "reason": reason,
                "elapsed_ms": report["elapsed_ms"],
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--method", choices=("auto", "cone", "lp"), default="auto")
    parser.add_argument("--prior-limit", type=float, default=0.5)
    parser.add_argument("--plot", action="store_true")
    parser.add_argument("--allocation-trace", type=Path)
    main(parser.parse_args())
