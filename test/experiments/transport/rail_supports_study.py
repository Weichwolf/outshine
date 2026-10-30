"""Whole bounded elevated-rail supports from a verified retained deck/world."""

import argparse
import json
import os
import platform
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from process_memory import peak_rss_bytes
from rail_supports_context import DEFAULT_FIXTURE, load
from rail_supports_plan import VARIANTS, plan
from stair_terrain_experiment import frozen
from wien_support_experiment import packed


def choose(methods):
    complete = [
        name
        for name, note in methods.items()
        if note["accepted"]
        and all(s["within_recipe_span"] for s in note["source_spacing"])
    ]
    return max(complete or list(methods), key=lambda name: methods[name]["accepted"])


def run(args):
    commit = frozen()
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Fresh private system-temp support output required")
    cpu, wall, load_before = time.process_time(), time.perf_counter(), os.getloadavg()
    context, rows, base = load(args.deck, args.fixture)
    prepared_cpu, prepared_wall = time.process_time() - cpu, time.perf_counter() - wall
    args.output.mkdir(parents=True)
    methods = {}
    for method in VARIANTS:
        products, note = plan(context, rows, method=method)
        data = packed(
            [p.body for p in products],
            [(p.lower_contact, p.upper_contact) for p in products],
        )
        path = args.output / (method + ".npz")
        np.savez_compressed(path, **data)
        note.update(
            mesh_sha256=digest(path),
            held_output_array_bytes=sum(v.nbytes for v in data.values()),
        )
        methods[method] = note
        print(
            json.dumps(
                {k: note[k] for k in ("method", "accepted", "cpu_seconds", "seconds")}
            ),
            flush=True,
        )
    selected = choose(methods)
    report = {
        "commit": commit,
        "deck": str(args.deck.resolve()),
        "deck_report_sha256": digest(args.deck / "report.json"),
        "deck_candidates_sha256": base["candidates_sha256"],
        "fixture_sha256": digest(args.fixture),
        "origin_lat_lon": base["origin_lat_lon"],
        "old_source_ids": base["old_source_ids"],
        "rail_candidate_source_ids": base["source_ids"],
        "old_road_arrays": base["old_road_arrays"],
        "context_work": context.work,
        "methods": methods,
        "selected": selected,
        "complete_recipe_spans": all(
            s["within_recipe_span"] for s in methods[selected]["source_spacing"]
        ),
        "admitted_source_ids": [],
        "cost": {
            "prepare_cpu_s": prepared_cpu,
            "prepare_wall_s": prepared_wall,
            "total_cpu_s": time.process_time() - cpu,
            "total_wall_s": time.perf_counter() - wall,
            "process_peak_rss_bytes": peak_rss_bytes(),
            "loadavg_before": load_before,
            "nice": os.getpriority(os.PRIO_PROCESS, 0),
            "platform": platform.platform(),
        },
        "scope": "Whole procedural support station/contact candidates on unchanged complete114 world and retained elevated deck. Local omissions preserve baseline and source reasons. Cooperative eight-second variant budgets; no structural/foundation/railhardware/outerrouting/source/native/world/consumer/device/UMA admission.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--deck", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, default=DEFAULT_FIXTURE)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
