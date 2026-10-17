"""Equal whole-source landing construction, physical contacts and allocation phases."""

import argparse
import contextlib
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
from stair_landing_contacts import node_profiles
from stair_landing_pads import complete_landings
from stair_network_geometry import interactions, vertex_links
from stair_terrain_experiment import frozen


def measure(args):
    commit = frozen()
    body_path = args.output.with_suffix(".npz")
    if any(p.exists() for p in (args.output, body_path)) or (
        args.profile and args.profile.exists()
    ):
        raise ValueError(
            "fresh explicit landing outputs and allocation profile required"
        )
    recipe = json.loads(args.fixture.read_text())
    _, rows, nodes, stairs, _, _ = load(args.fixture)
    for row in rows:
        row.pop("junction_pads_xy_m", None)
    apply_pads(rows, nodes, recipe["junction_pads"])
    settings = dict(recipe["complete_chosen_landings"], method=args.method)
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=True
        )
    began = time.perf_counter()
    with tracker:
        pads, ledger = complete_landings(rows, nodes, stairs, **settings)
        plan = layout(rows, "width", junctions=nodes, stairs=stairs)
        heights, solver = solve(plan)
        if heights is None:
            raise ValueError("bounded whole landing fit failed: " + str(solver))
        _, data, geometry = build(plan, heights)
    seconds = time.perf_counter() - began
    pairs, _ = interactions(rows, nodes)
    physical = audit(
        data,
        [corridor(r) for r in rows],
        limits=[r["gradient"] for r in rows],
        crossings=pairs,
    )
    whole = node_profiles(data, rows, nodes)
    if not whole["all_chosen_internal_profiles_whole"]:
        raise ValueError("cost candidate misses actual whole chosen internal profiles")
    prior = contacts(data, rows, bindings(args.fixture, rows))
    links = vertex_links(data["body_vertices"], data["body_faces"])
    peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    np.savez_compressed(body_path, **data)
    report = {
        "commit": commit,
        "fixture_sha256": digest(args.fixture),
        "source_ids": [r["source_id"] for r in rows],
        "method": args.method,
        "pads": pads,
        "selection": ledger,
        "generation_seconds": seconds,
        "process_peak_rss_bytes": peak,
        "body_sha256": digest(body_path),
        "owned_body_array_bytes": sum(a.nbytes for a in data.values()),
        "geometry": geometry,
        "solver": solver,
        "physical": physical,
        "whole_landing_contacts": whole,
        "previous_pair_contacts": prior,
        "vertex_links": links,
        "scope": "One fresh worker. Timed/tracked: bounded landing discovery/pads, arrangement, LP and closed body after imports/input decode and unchanged prior ground corner. Actual whole physical/profile checks and output IO outside tracker; process peak includes checks. No terrain, piers, renderer or native/device proof. Instrumented/normal times stay separate; internal total copies unknown.",
    }
    if args.profile:
        report["allocations"] = dict(profile_note(args.profile), scope=report["scope"])
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps({k: report[k] for k in ("commit", "method", "generation_seconds")})
    )


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixture", type=Path, default=FIXTURE.with_name("stair-landings-vienna.json")
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
