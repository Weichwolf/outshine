"""Bounded cycle-ramp alternatives connect three real stairs and two foot bridges."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from stair_context_experiment import frontier_ends as context_ends
from stair_context_experiment import stage
from stair_context_inputs import FIXTURE as CONTEXT
from stair_context_inputs import PROFILES
from stair_context_inputs import load as context_inputs
from stair_junction_layout import INPUT, layout
from stair_junction_solve import solve
from stair_network_geometry import interactions, vertex_links
from stair_network_inputs import FIXTURE, load


def outer_ends(rows):
    expected = (
        ("openfreemap:14/8937/5678:transportation:39881120:26", -1),
        ("openfreemap:14/8937/5678:transportation:97041640:6", 0),
    )
    keys = [r["source_id"] for r in rows]
    return [(keys.index(key), end) for key, end in expected]


def alternatives(args):
    result = []
    for offset in (0, 6, 12, 18):
        _case, rows, nodes, stairs, _context, recipe = load(
            args.fixture, args.context, args.native_case, offset=offset
        )
        plan = layout(rows, "width", junctions=nodes, stairs=stairs)
        h, note = solve(plan)
        variant = {"offset_m": offset, "solve": note}
        if offset == 0 and h is None:
            _field, capacity = solve(plan, gap_m=None)
            if not capacity["valid"]:
                raise ValueError("original finite capacity solve failed")
            variant["capacity"] = capacity
        result.append(variant)
        if h is not None:
            if offset != recipe["detour"]["offset_m"]:
                raise ValueError(
                    "chosen recipe skipped an earlier feasible bounded alternative"
                )
            break
    if h is None:
        raise ValueError("bounded ramp alternatives cannot connect this local network")
    return result


def cameras(args, name):
    target = args.output / name
    for view in ("overview", "diagnostic"):
        path = target / (view + "-scene.json")
        row = json.loads(path.read_text())
        row["id"] = "Wien-stair-network-" + name + "-" + view
        path.write_text(json.dumps(row, indent=2) + "\n")
    row = json.loads((target / "diagnostic-scene.json").read_text())
    row.update(
        id="Wien-stair-network-" + name + "-west",
        eye=[-35, -75, 200],
        target=[4, -17, 166],
        span_m=110,
        output=str(target / "west.png"),
    )
    (target / "west-scene.json").write_text(json.dumps(row, indent=2) + "\n")


def generate(args):
    start = time.perf_counter()
    if (
        args.output.exists()
        or subprocess.check_output(
            [
                "git",
                "status",
                "--porcelain",
                "--",
                "lib",
                str(Path(__file__).resolve().parent),
            ],
            text=True,
        ).strip()
    ):
        raise ValueError("fresh network output and frozen clean research required")
    variants = alternatives(args)
    _case, original, original_nodes, context = context_inputs(
        args.context, args.native_case
    )
    _case, rows, nodes, stairs, _context, recipe = load(
        args.fixture, args.context, args.native_case
    )
    args.output.mkdir(parents=True)
    before = stage(
        args, "before", original, original_nodes, context_ends(context, original)
    )
    after = stage(args, "after", rows, nodes, outer_ends(rows), stairs)
    pairs, proof = interactions(rows, nodes)
    if pairs != after["crossings"]:
        raise ValueError(
            "actual whole noncontact overlaps differ from audited crossings"
        )
    with np.load(args.output / "after/body.npz") as data:
        proof.update(vertex_links(data["body_vertices"], data["body_faces"]))
    after["whole_interactions"] = proof
    for phase in ("before", "after"):
        cameras(args, phase)
    keys = [r["source_id"] for r in rows]
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "native_case_sha256": digest(args.native_case),
        "fixture_sha256": digest(args.fixture),
        "context_sha256": digest(args.context),
        "profile_recipe_sha256": digest(PROFILES),
        "before": before,
        "after": after,
        "alternatives": variants,
        "candidate_source_ids": keys,
        "admitted_source_ids": [],
        "unadmitted_context_source_ids": [
            r["source_id"] for r in context["sources"] if r["source_id"] not in keys
        ],
        "changed_source_ids": [recipe["detour"]["source_id"]],
        "planned_sources": [
            {
                "source_id": r["source_id"],
                "xy_m": r["xy"].tolist(),
                "prior_m": r["prior"].tolist(),
            }
            for r in rows
        ],
        "seconds": time.perf_counter() - start,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "status": "candidate_requires_remaining_context_clearance_and_two_outer_contacts",
        "scope": "Three actual stair bodies and full selected band interactions. One procedural cycle detour retains all actual shared nodes and all original source data. No full-context, actual terrain/outer-contact or native recovery admission.",
    }
    (args.output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {k: result[k] for k in ("commit", "seconds", "peak_rss_kib", "status")}
        ),
        flush=True,
    )


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=FIXTURE)
    parser.add_argument("--context", type=Path, default=CONTEXT)
    parser.add_argument("--native-case", type=Path, default=INPUT)
    parser.add_argument("--palette-scene", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    generate(arguments())
