"""Compare explicit corner recipes on the same whole sources and physical checks."""

import argparse
import contextlib
import copy
import json
import resource
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from rail_instance_cost import profile_note
from shared_bridge_geometry import corridor
from stair_corner_geometry import apply_pads
from stair_cycle_contacts import bindings, contacts
from stair_cycle_inputs import FIXTURE, load
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import layout
from stair_junction_solve import solve
from stair_network_geometry import interactions, vertex_links
from stair_terrain_experiment import frozen


def measure(args):
    commit = frozen()
    if args.output.exists() or (args.profile and args.profile.exists()):
        raise ValueError("fresh explicit comparison output/profile required")
    _, rows, nodes, stairs, context, _ = load(args.fixture)
    for row in rows:
        row.pop("junction_pads_xy_m", None)
    declarations = copy.deepcopy(json.loads(args.fixture.read_text())["junction_pads"])
    for declaration in declarations:
        declaration["method"] = args.method
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=True
        )
    began = time.perf_counter()
    with tracker:
        pads = apply_pads(rows, nodes, declarations)
        plan = layout(rows, "width", junctions=nodes, stairs=stairs)
        heights, solver = solve(plan)
        if heights is None:
            raise ValueError("bounded whole recipe fit failed: " + str(solver))
        _, data, geometry = build(plan, heights)
    generation_seconds = time.perf_counter() - began
    pairs, _ = interactions(rows, nodes)
    physical = audit(
        data,
        [corridor(r) for r in rows],
        limits=[r["gradient"] for r in rows],
        crossings=pairs,
    )
    joins = contacts(data, rows, bindings(args.fixture, rows))
    links = vertex_links(data["body_vertices"], data["body_faces"])
    peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    report = {
        "commit": commit,
        "fixture_sha256": digest(args.fixture),
        "source_ids": [r["source_id"] for r in rows],
        "method": args.method,
        "pads": pads,
        "generation_seconds": generation_seconds,
        "process_peak_rss_bytes": peak,
        "owned_body_array_bytes": sum(a.nbytes for a in data.values()),
        "geometry": geometry,
        "solver": solver,
        "physical": physical,
        "whole_profile_contacts": joins,
        "vertex_links": links,
        "source_payload_sha256": context["vector"]["sha256"],
        "scope": "One fresh worker. Timed/tracked: pads, arrangement, bounded LP and closed body after input decode. Actual whole geometry/profile checks outside timing; process peak includes them. No terrain, piers, renderer or native/device proof. Normal and instrumented times stay separate; internal copy totals unknown.",
    }
    if args.profile:
        note = profile_note(args.profile)
        note["scope"] = report["scope"]
        report["allocations"] = note
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps({k: report[k] for k in ("commit", "method", "generation_seconds")})
    )


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixture",
        type=Path,
        default=FIXTURE.with_name("stair-cycle-corner-vienna.json"),
    )
    parser.add_argument(
        "--method", choices=("profile_hull", "profile_box"), required=True
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    measure(arguments())
