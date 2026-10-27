"""Reproduce a source-checked experimental Ground star, without shared admission."""

import argparse
import json
import math
import subprocess
import time
from contextlib import nullcontext
from dataclasses import replace
from pathlib import Path

import numpy as np
from branched_ground_source import checked_star, source_input
from branched_ground_terminals import construct
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground import fit_ground
from replay_canonical_bridge_ports import allocations
from replay_canonical_ports import serializable

ROOT = Path(__file__).resolve().parents[3]
INPUT = Path(__file__).parent / "fixtures/bridge-source-facts"
SEED = (5699117369358000712, 1)


def run(
    bundle,
    terrain,
    *,
    budget=None,
    proximal=True,
    cut_method="linear",
    prior_limit=0.5,
    method="auto",
):
    if not math.isfinite(prior_limit) or not 0 < prior_limit <= 1000:
        raise ValueError("explicit finite positive prior limit required")
    if method not in ("auto", "cone", "lp"):
        raise ValueError("known diagnostic solver method required")
    source, groups, ways, binding, _ = bundle
    budget = PortBudget() if budget is None else budget
    began = time.perf_counter()
    deadline = time.monotonic() + budget.surface.time_limit_s
    index, edges, centre, leaves, guards = checked_star(
        source, SEED, groups, ways, binding, budget
    )
    anchor, inputs, caps, geometry = source_input(
        source,
        index,
        edges,
        centre,
        leaves,
        terrain,
        budget,
        proximal=proximal,
        cut_method=cut_method,
        deadline=deadline,
    )
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise ValueError("diagnostic_ground_total_deadline")
    local_budget = replace(
        budget,
        surface=replace(
            budget.surface,
            time_limit_s=remaining,
            solver_time_limit_s=min(budget.surface.solver_time_limit_s, remaining),
            geometry_time_limit_s=min(budget.surface.geometry_time_limit_s, remaining),
        ),
    )
    field, fitting = fit_ground(inputs, prior_limit, method, local_budget)
    if time.monotonic() >= deadline:
        raise ValueError("diagnostic_ground_total_deadline")
    report = {
        "state": "Pending",
        "source_guards": guards,
        "geometry": geometry,
        "field": fitting,
        "maximum_prior_change_m": prior_limit,
        "source_binding": source.binding,
        "candidate_binding": binding,
        "scope": "Experimental finite geometry only; shared scope/terminal producers and Source/Unknown are unchanged. No bridge/Plan/World/device qualification.",
    }
    mesh = None
    if field is not None:
        if not hasattr(field, "plan"):
            raise ValueError("this diagnostic currently requires a C1 finite plan")
        mesh, checked = construct(
            anchor, field, inputs, caps, index, budget, prior_limit=prior_limit
        )
        report.update(state="FiniteGeometryDiagnostic", terminals=checked)
    if time.monotonic() >= deadline:
        raise ValueError("diagnostic_ground_total_deadline")
    report["elapsed_ms"] = 1000 * (time.perf_counter() - began)
    return report, inputs, field, mesh


def plot(output, inputs, mesh, report):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection
    from outshine_ref.transport.canonical_ports._geodesy import frame

    fig = plt.figure(figsize=(15, 5), layout="constrained")
    for position, zoom in ((1, False), (2, True)):
        axis = fig.add_subplot(1, 3, position)
        axis.add_collection(
            PolyCollection(
                inputs.surface.vertices[inputs.surface.triangles, :2],
                array=inputs.face_parts,
                cmap="tab20",
                edgecolors=".4",
                linewidths=0.6,
            )
        )
        axis.scatter(*inputs.surface.vertices[:, :2].T, s=8, color="black")
        axis.autoscale()
        axis.set_aspect("equal")
        axis.set(xlabel="Source east [m]", ylabel="Source north [m]")
        if zoom:
            axis.set(xlim=(-3, 3), ylim=(-3, 3), title="Whole cuts and short side arm")
    axis = fig.add_subplot(133, projection="3d")
    axis.plot_trisurf(
        *mesh.vertices.T, triangles=mesh.triangles, cmap="terrain", linewidth=0
    )
    axis.scatter(*inputs.surface.vertices.T, color="black", s=9)
    axis.set(xlabel="East [m]", ylabel="North [m]", zlabel="Source height [m]")
    fig.suptitle(
        "Actual V2 / DEM experimental three-arm field; no shared source/World admission"
    )
    fig.savefig(output / "geometry.png", dpi=110)
    plt.close(fig)
    fig, axes = plt.subplots(1, 3, figsize=(13, 4), layout="constrained")
    origin, basis = frame(report["geometry"]["source_anchor"])
    for axis, item in zip(axes, report["terminals"]["values"], strict=True):
        cap, port = item["source_terminal"], item["port"]
        target, target_basis = frame(cap["anchor_longitude_latitude"])
        at, rotation = basis.T @ (target - origin), basis.T @ target_basis
        own = (mesh.vertices[list(cap["cap_vertices"])] - at) @ rotation
        direction = np.array(port["direction_EN"])
        left = np.array([-direction[1], direction[0]])
        transverse = own[:, :2] @ left
        order = np.argsort(transverse)
        axis.plot(transverse[order], own[order, 2], "o-")
        axis.scatter([0], [port["height_m"]], marker="+", color="C1")
        axis.set(
            xlabel="Port-left [m]",
            ylabel="Port-ENU height [m]",
            title="Original group " + cap["group"].split(":")[-1],
            ylim=(port["height_m"] - 0.2, port["height_m"] + 0.2),
        )
        axis.ticklabel_format(axis="y", style="plain", useOffset=False)
    fig.savefig(output / "sections.png", dpi=110)
    plt.close(fig)


def main(args):
    args.output.mkdir(parents=True, exist_ok=False)
    bundle = load_bridge_source_facts(INPUT)
    manifest = json.loads((INPUT / "manifest.json").read_text())
    terrain, terrain_binding = terrain_input(
        FIXTURE, {"terrain": manifest["terrain_reference"]}
    )
    tracking = nullcontext()
    if args.allocation_trace:
        import memray

        tracking = memray.Tracker(
            str(args.allocation_trace), trace_python_allocators=True
        )
    with tracking:
        report, inputs, field, mesh = run(
            bundle,
            terrain,
            proximal=not args.no_proximal,
            cut_method=args.cuts,
            prior_limit=args.prior_limit,
            method=args.method,
        )
    report.update(
        commit=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        working_tree_dirty=bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=ROOT, text=True
            )
        ),
        terrain_binding=terrain_binding,
        platform="shared Linux/Python host; single diagnostic, no native/device proof",
    )
    if args.allocation_trace:
        report["allocations"] = allocations(args.allocation_trace)
        report["allocations"]["scope"] = (
            "Decoded inputs -> source index/guards, geometry, actual DEM, fit and three terminal audits. Driver decode/imports before run excluded; no process peak or total copied-byte proof."
        )
    np.savez_compressed(
        args.output / "prior.npz",
        vertices=inputs.surface.vertices,
        faces=inputs.surface.triangles,
        parts=inputs.face_parts,
        frames=inputs.grade_frames,
    )
    if mesh is not None:
        np.savez_compressed(
            args.output / "finite.npz",
            vertices=mesh.vertices,
            faces=mesh.triangles,
            parts=field.plan.face_parts,
            source_faces=field.plan.source_faces,
            original_vertex_map=field.plan.original_vertex_map,
            coefficients=field.plan.plane_coefficients,
            datum=field.plan.datum_m,
        )
        if args.plot:
            plot(args.output, inputs, mesh, report)
    (args.output / "report.json").write_text(
        json.dumps(serializable(report), indent=2) + "\n"
    )
    print(
        json.dumps(
            {
                "state": report["state"],
                "reason": report["field"].get("reason"),
                "prior_error_m": report["field"].get("maximum_original_prior_error_m"),
                "elapsed_ms": report["elapsed_ms"],
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cuts", choices=("linear", "common"), default="linear")
    parser.add_argument("--no-proximal", action="store_true")
    parser.add_argument("--prior-limit", type=float, default=0.5)
    parser.add_argument("--method", choices=("auto", "cone", "lp"), default="auto")
    parser.add_argument("--plot", action="store_true")
    parser.add_argument("--allocation-trace", type=Path)
    main(parser.parse_args())
