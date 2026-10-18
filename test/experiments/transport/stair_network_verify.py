"""Saved three-stair body/field/source/contact replay without solving or remeshing."""

import json
import resource
import subprocess
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from stair_context_experiment import frontier_ends as context_ends
from stair_context_inputs import PROFILES
from stair_context_inputs import load as context_inputs
from stair_context_verify import stage
from stair_network_experiment import arguments, outer_ends
from stair_network_geometry import interactions, vertex_links
from stair_network_inputs import load


def verify(args):
    if subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--",
            "lib",
            str(Path(__file__).resolve().parent),
        ],
        text=True,
    ).strip():
        raise ValueError("network replay requires frozen clean research")
    report = json.loads((args.output / "comparison.json").read_text())
    if report["dirty"] or any(
        report[key] != digest(path)
        for key, path in (
            ("fixture_sha256", args.fixture),
            ("context_sha256", args.context),
            ("native_case_sha256", args.native_case),
            ("profile_recipe_sha256", PROFILES),
        )
    ):
        raise ValueError("actual network inputs/profile changed")
    _case, original, original_nodes, context = context_inputs(
        args.context, args.native_case
    )
    _case, rows, nodes, stairs, _context, recipe = load(
        args.fixture, args.context, args.native_case
    )
    keys = [r["source_id"] for r in rows]
    planned = [
        {
            "source_id": r["source_id"],
            "xy_m": r["xy"].tolist(),
            "prior_m": r["prior"].tolist(),
        }
        for r in rows
    ]
    if (
        report["admitted_source_ids"]
        or report["candidate_source_ids"] != keys
        or report["planned_sources"] != planned
        or report["changed_source_ids"] != [recipe["detour"]["source_id"]]
        or report["unadmitted_context_source_ids"]
        != [r["source_id"] for r in context["sources"] if r["source_id"] not in keys]
    ):
        raise ValueError("network source/changed-axis/unadmitted ownership changed")
    result = {
        "before": stage(
            args,
            "before",
            original,
            original_nodes,
            report["before"],
            context_ends(context, original),
        ),
        "after": stage(
            args,
            "after",
            rows,
            nodes,
            report["after"],
            outer_ends(rows),
            [s[0] for s in stairs],
        ),
    }
    pairs, whole = interactions(rows, nodes)
    with np.load(args.output / "after/body.npz") as data:
        whole.update(vertex_links(data["body_vertices"], data["body_faces"]))
    if (
        pairs != report["after"]["crossings"]
        or whole != report["after"]["whole_interactions"]
    ):
        raise ValueError("actual complete band-interaction/topology proof changed")
    result.update(
        valid_local_bodies=True,
        admitted_source_ids=[],
        status=report["status"],
        generation_commit=report["commit"],
        reader_commit=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
    )
    (args.output / "verification.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    verify(arguments())
