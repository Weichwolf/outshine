"""Replay actual full-width neighbour caps and complete input inventory; no solve."""

import json
import resource
import subprocess
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from stair_context_inputs import FIXTURE as CONTEXT
from stair_context_inputs import PROFILES
from stair_context_verify import stage
from stair_junction_layout import INPUT
from stair_network_experiment import outer_ends as network_ends
from stair_network_geometry import interactions, vertex_links
from stair_network_inputs import FIXTURE as NETWORK
from stair_network_inputs import load as network_inputs
from stair_outer_context import catalog
from stair_outer_experiment import arguments
from stair_outer_geometry import bound_contacts, outer_ends
from stair_outer_inputs import load


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
        raise ValueError("outer-neighbour replay requires frozen clean research")
    report = json.loads((args.output / "comparison.json").read_text())
    if report["dirty"] or any(
        report[key] != digest(path)
        for key, path in (
            ("fixture_sha256", args.fixture),
            ("context_sha256", CONTEXT),
            ("network_sha256", NETWORK),
            ("native_case_sha256", INPUT),
            ("profile_recipe_sha256", PROFILES),
            ("source_catalog_sha256", args.output / "source-catalog.json"),
        )
    ):
        raise ValueError("actual outer-neighbour input/profile/catalog binding changed")
    case, rows, nodes, stairs, context, _recipe = load(args.fixture)
    original_catalog = catalog(case, rows, context)
    if original_catalog != json.loads(
        (args.output / "source-catalog.json").read_text()
    ):
        raise ValueError("original complete payload/source/area inventory changed")
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
        or report["candidate_source_ids"] != [r["source_id"] for r in rows]
        or report["changed_source_ids"] != [rows[6]["source_id"]]
        or report["planned_sources"] != planned
        or report["unadmitted_line_parts"]
        != len(original_catalog["unadmitted_sources"])
        or report["unadmitted_area_features"]
        != len(original_catalog["unadmitted_non_line_features"])
    ):
        raise ValueError("actual changed/unadmitted source ownership changed")
    _case, original, old_nodes, old_stairs, _context, _network = network_inputs()
    result = {
        "before": stage(
            args,
            "before",
            original,
            old_nodes,
            report["before"],
            network_ends(original),
            [s[0] for s in old_stairs],
        ),
        "after": stage(
            args,
            "after",
            rows,
            nodes,
            report["after"],
            outer_ends(rows, nodes),
            [s[0] for s in stairs],
        ),
    }
    pairs, whole = interactions(rows, nodes)
    with np.load(args.output / "after/body.npz") as saved:
        data = dict(saved)
    whole.update(vertex_links(data["body_vertices"], data["body_faces"]))
    if (
        pairs != report["after"]["crossings"]
        or whole != report["after"]["whole_interactions"]
        or bound_contacts(data, rows) != report["after"]["bound_former_profiles"]
    ):
        raise ValueError("actual whole neighbour contact/interactions changed")
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
