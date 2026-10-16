"""A whole eastern cycle loop, preserving previous cases as explicit regressions."""

import argparse
import json
import resource
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from stair_blend_display import scenes
from stair_blend_experiment import stage as terrain_stage
from stair_blend_plan import prepare
from stair_context_experiment import stage
from stair_cycle_contacts import contacts
from stair_cycle_inputs import FIXTURE, load
from stair_network_geometry import interactions, vertex_links
from stair_outer_context import catalog
from stair_outer_experiment import cameras
from stair_outer_geometry import outer_ends
from stair_terrain_experiment import PALETTE, frozen


def base(args):
    report = json.loads((args.base_terrain / "comparison.json").read_text())
    replay = json.loads((args.base_terrain / "verification.json").read_text())
    if (
        not replay["valid_local_terrain"]
        or replay["products_report_sha256"]
        != digest(args.base_terrain / "comparison.json")
        or Path(report["network_products"]).resolve() != args.base_network.resolve()
        or report["source_body_sha256"] != digest(args.base_network / "after/body.npz")
        or report["display"]["after_display_sha256"]
        != digest(args.base_terrain / "after-display.npz")
    ):
        raise ValueError("verified original eighteen-source terrain baseline required")
    for name, expected in report["artifacts"].items():
        if digest(args.base_terrain / name) != expected:
            raise ValueError("original terrain baseline artifact changed")
    return {
        "network": str(args.base_network.resolve()),
        "terrain": str(args.base_terrain.resolve()),
        "network_report_sha256": digest(args.base_network / "comparison.json"),
        "terrain_report_sha256": digest(args.base_terrain / "comparison.json"),
        "display_sha256": report["display"]["after_display_sha256"],
    }


def presentation(args, rows, body, terrain, roles):
    cameras(args, body.vertices)
    target = np.r_[np.mean(rows[-1]["xy"], axis=0), np.mean(body.vertices[:, 2])]
    diagnostic = args.output / "after/diagnostic-scene.json"
    row = json.loads(diagnostic.read_text())
    row.update(
        target=target.tolist(), eye=(target + [240, -320, 260]).tolist(), span_m=420
    )
    diagnostic.write_text(json.dumps(row, indent=2) + "\n")
    report = scenes(args.output, args.output, [("after", terrain, roles)])
    before = args.output / "before-display.npz"
    before.symlink_to(args.base_terrain.resolve() / "after-display.npz")
    report["before_display_sha256"] = digest(before)
    for view in ("overview", "diagnostic"):
        path = args.output / ("after-" + view + "-scene.json")
        after = json.loads(path.read_text())
        after["id"] = "Wien-cycle-extension-after-" + view
        path.write_text(json.dumps(after, indent=2) + "\n")
        after.update(
            id="Wien-cycle-extension-before-" + view,
            mesh=str(before),
            mesh_sha256=digest(before),
            output=str(args.output / ("before-" + view + ".png")),
        )
        (args.output / ("before-" + view + "-scene.json")).write_text(
            json.dumps(after, indent=2) + "\n"
        )
    return report


def generate(args):
    commit = frozen()
    baseline = base(args)
    if args.output.exists():
        raise ValueError("fresh explicit cycle extension output required")
    began = time.perf_counter()
    case, rows, nodes, stairs, context, sample = load()
    args.output.mkdir(parents=True)
    network = stage(args, "after", rows, nodes, outer_ends(rows, nodes), stairs)
    data = dict(np.load(args.output / "after/body.npz"))
    pairs, whole = interactions(rows, nodes)
    if pairs != network["crossings"]:
        raise ValueError("cycle whole band and axis crossings disagree")
    whole.update(vertex_links(data["body_vertices"], data["body_faces"]))
    body = Mesh(data["body_vertices"], data["body_faces"])
    _initial, layout, ground, upper, footprint, plan = prepare(
        data, rows, context, None, sample
    )
    np.savez_compressed(args.output / "terrain-plan.npz", **layout)
    terrain, roles, proof = terrain_stage(
        args.output, "membrane", layout, ground, upper, footprint, 16
    )
    if terrain is None:
        raise ValueError("extended cycle terrain failed actual export admission")
    inventory = catalog(case, rows, context)
    (args.output / "source-catalog.json").write_text(
        json.dumps(inventory, indent=2) + "\n"
    )
    report = {
        "commit": commit,
        "fixture_sha256": digest(FIXTURE),
        "baseline": baseline,
        "after": network,
        "whole_interactions": whole,
        "whole_profile_contacts": contacts(data, rows),
        "terrain_plan": plan,
        "terrain": proof,
        "source_catalog_sha256": digest(args.output / "source-catalog.json"),
        "candidate_source_ids": [r["source_id"] for r in rows],
        "admitted_source_ids": [],
        "seconds_before_display": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Whole nineteen-source local body and continuous terrain; joint heights replanned, all original axes except the existing folded ramp unchanged. Two former eastern ports bound, three old and two new outer profiles remain open. Indoor, remaining source/level, user-envelope, statutory and native admission remain open.",
        "display": presentation(args, rows, body, terrain, roles),
    }
    report["artifacts"] = {
        name: digest(args.output / name)
        for name in ("terrain-plan.npz", "membrane-field.npz", "membrane-body.npz")
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {k: report[k] for k in ("commit", "seconds_before_display", "scope")}
        )
    )


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-network", type=Path, required=True)
    parser.add_argument("--base-terrain", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--palette-scene", type=Path, default=PALETTE)
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    generate(arguments())
