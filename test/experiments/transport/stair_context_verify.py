"""Replay both saved source extents; no candidate triangulation or solve."""

import json
import resource
import subprocess
from pathlib import Path
from types import SimpleNamespace

import numpy as np
from camera_height_inputs import digest
from shared_bridge_geometry import corridor
from stair_context_experiment import arguments, crossings, frontier_ends
from stair_context_inputs import PROFILES, load
from stair_junction_audit import audit
from stair_junction_experiment import outer_ports
from stair_junction_layout import inputs
from stair_junction_verify import display_proof, landing_proof, source_fields


def stage(args, name, rows, junctions, report, ends=None):
    target = args.output / name
    for file, key in (
        ("body.npz", "body_sha256"),
        ("height-input.npz", "height_input_sha256"),
    ):
        if digest(target / file) != report[key]:
            raise ValueError("saved contextual body/field binding changed")
    with np.load(target / "body.npz") as d:
        data = dict(d)
    with np.load(target / "height-input.npz") as d:
        arrays = dict(d)
    domains = [corridor(r) for r in rows]
    pairs = crossings(rows)
    if (
        pairs != report["crossings"]
        or [r["source_id"] for r in rows] != report["source_ids"]
    ):
        raise ValueError("saved actual source/crossing ownership changed")
    proof = audit(data, domains, limits=[r["gradient"] for r in rows], crossings=pairs)
    fields = source_fields(arrays, data, rows, domains)
    proof["landings"] = landing_proof(fields, rows, domains, junctions)
    proposed = outer_ports(
        {
            "rows": rows,
            "xy": arrays["xy"],
            "parts": [arrays["faces" + str(i)] for i in range(len(rows))],
        },
        arrays["heights"],
        ends,
    )
    if proposed != report["proposed_outer_profiles"]:
        raise ValueError("saved actual outer profiles changed")
    display_proof(
        SimpleNamespace(output=target, palette_scene=args.palette_scene), data, report
    )
    return proof


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
        raise ValueError("context replay requires frozen clean sources")
    report = json.loads((args.output / "comparison.json").read_text())
    if (
        report["dirty"]
        or report["fixture_sha256"] != digest(args.fixture)
        or report["native_case_sha256"] != digest(args.native_case)
        or report["profile_recipe_sha256"] != digest(PROFILES)
    ):
        raise ValueError("actual source/context/profile binding changed")
    _case, rows, junctions, fixture = load(args.fixture, args.native_case)
    _case, original = inputs(args.native_case)
    if (
        report["admitted_source_ids"]
        or report["candidate_source_ids"] != fixture["selected_source_ids"]
        or report["unadmitted_context_source_ids"]
        != [
            r["source_id"]
            for r in fixture["sources"]
            if r["role"] == "context_not_admitted"
        ]
        or report["frontier"] != fixture["frontier"]
    ):
        raise ValueError(
            "unverified context/outer sources were lost or silently admitted"
        )
    proof = {
        "before": stage(args, "before", original, None, report["before"]),
        "after": stage(
            args,
            "after",
            rows,
            junctions,
            report["after"],
            frontier_ends(fixture, rows),
        ),
        "valid_local_bodies": True,
        "admitted_source_ids": [],
        "status": report["status"],
        "generation_commit": report["commit"],
        "reader_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
    }
    (args.output / "verification.json").write_text(json.dumps(proof, indent=2) + "\n")
    print(json.dumps(proof), flush=True)


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    verify(arguments())
