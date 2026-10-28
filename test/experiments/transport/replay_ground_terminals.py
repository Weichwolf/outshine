"""Actual source-bound finite terminal geometry, fields, costs and local diagnosis."""

import argparse
import json
import time
from contextlib import nullcontext
from pathlib import Path

import numpy as np
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from outshine_ref.transport.canonical_ports._geodesy import frame
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)
from outshine_ref.transport.canonical_ports._ground_terminals import (
    derive_scoped_ground_terminals,
)
from replay_canonical_bridge_ports import allocations
from replay_canonical_ports import serializable


def plot(output, prepared, report):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    mesh, prior = report["surface"], report["source_prior_surface"]
    figure = plt.figure(figsize=(15, 4.5), layout="constrained")
    axis = figure.add_subplot(131, projection="3d")
    axis.plot_trisurf(
        *prior.vertices.T, triangles=prior.triangles, color="#888888", alpha=0.3
    )
    axis.plot_trisurf(
        *mesh.vertices.T, triangles=mesh.triangles, color="#176ba0", alpha=0.8
    )
    axis.set(
        xlabel="Source east (m)",
        ylabel="Source north (m)",
        title="Source ENU (m)\nOriginal priors and complete finite field",
    )
    origin, axes = frame(prepared[0])
    for i, value in enumerate(report["terminal_values"]):
        cap, port = value["source_terminal"], value["port"]
        target, target_axes = frame(cap["anchor_longitude_latitude"])
        at, basis = axes.T @ (target - origin), axes.T @ target_axes
        vertices = mesh.vertices[list(cap["cap_vertices"])]
        own = (vertices - at) @ basis
        direction = np.asarray(port["direction_EN"])
        left = np.array([-direction[1], direction[0]])
        station = own[:, :2] @ left
        order = np.argsort(station)
        axis = figure.add_subplot(1, 3, i + 2)
        axis.plot(station[order], own[order, 2], "o-", label="actual finite section")
        axis.scatter(
            [0],
            [port["height_m"]],
            marker="+",
            color="#d65a00",
            s=120,
            label="bound source ray / end plane",
        )
        axis.set(
            xlabel="Port-left offset (m)",
            ylabel="Port-ENU height (m)",
            title="Original group " + cap["group"].split(":")[-1],
            xlim=(-7, 7),
            ylim=(port["height_m"] - 0.2, port["height_m"] + 0.2),
        )
        axis.ticklabel_format(axis="y", style="plain", useOffset=False)
        axis.grid(alpha=0.25)
        axis.legend(fontsize=8)
    figure.suptitle(
        "Bound terminal sections:3 m recipe, Source/Unknown unchanged, no Bridge/World join"
    )
    figure.savefig(output / "ground-terminals.png", dpi=150)
    plt.close(figure)


def main(args):
    args.output.mkdir(parents=True, exist_ok=False)
    source, groups, ways, binding, reader_work = load_bridge_source_facts(args.input)
    reference = json.loads((args.input / "manifest.json").read_text())[
        "terrain_reference"
    ]
    terrain, terrain_binding = terrain_input(FIXTURE, {"terrain": reference})
    start = time.perf_counter()
    index, reason, build_work = prepare_ground_scope_index(
        source, groups, ways, source_binding=source.binding, candidate_binding=binding
    )
    build_seconds = time.perf_counter() - start
    if reason:
        raise ValueError(reason)
    tracking = nullcontext()
    if args.allocation_trace:
        import memray

        tracking = memray.Tracker(
            str(args.allocation_trace), trace_python_allocators=True
        )
    width = next(
        span.width_m
        for span in source.spans
        if span.identifier == (8409897393650800292, 0)
    )
    start = time.perf_counter()
    with tracking:
        prepared, reason, report = derive_scoped_ground_terminals(
            source,
            (8409897393650800292, 0),
            groups,
            ways,
            terrain,
            source_binding=source.binding,
            candidate_binding=binding,
            terrain_binding=terrain_binding,
            maximum_prior_change_m=width / 4,
            method=args.method,
            scope_index=index,
        )
    seconds = time.perf_counter() - start
    if reason:
        raise ValueError(reason)
    result = {
        "scope": "Actual archived V2/DEM, explicit width/4 recipe; offline Python, no native/Bridge/terrain/World/device admission. Index separately timed; field includes source qualification,30 DEM observations, fitting, whole finite geometry requalification and terminal queries.",
        "method": args.method,
        "source_anchor": prepared[0],
        "index_build_seconds": build_seconds,
        "index_build_work": build_work,
        "field_seconds": seconds,
        "allocation_tracing": bool(args.allocation_trace),
        "reader_work": reader_work,
        "source_work": report["source_work"],
        "terrain_samples": report["terrain_samples"],
        "terminal_work": report["terminal_work"],
        "terminal_quality": report["terminal_quality"],
        "terminal_values": report["terminal_values"],
        "binding": report["binding"],
        "unknown_nodes_unchanged": len(source.unknown_nodes),
        "qualified_world_joins": report["qualified_world_joins"],
    }
    if args.allocation_trace:
        result["allocations"] = allocations(args.allocation_trace)
        result["allocations"]["scope"] = (
            "Field derivation only, existing index and decoded inputs excluded; traced Python allocator; allocation bytes are not copied bytes"
        )
    (args.output / "ground-terminals.json").write_text(
        json.dumps(serializable(result), indent=2) + "\n"
    )
    np.savez(
        args.output / "ground-terminals.npz",
        vertices=report["surface"].vertices,
        faces=report["surface"].triangles,
        source_faces=report["surface_source_faces"],
        prior_vertices=report["source_prior_surface"].vertices,
        prior_faces=report["source_prior_surface"].triangles,
    )
    if args.plot:
        plot(args.output, prepared, report)
    print(
        json.dumps(
            {
                "state": report["state"],
                "method": args.method,
                "seconds": seconds,
                "terminal_work": report["terminal_work"],
                "terminal_quality": report["terminal_quality"],
                "allocations": result.get("allocations"),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=Path(__file__).parent / "fixtures/bridge-source-facts",
    )
    parser.add_argument(
        "--method", choices=("auto", "affine", "cone", "lp"), default="auto"
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--allocation-trace", type=Path)
    parser.add_argument("--plot", action="store_true")
    main(parser.parse_args())
