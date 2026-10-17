"""Whole actual neighbouring ground bands with a checked folded cycle approach."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from stair_context_experiment import stage
from stair_context_inputs import FIXTURE as CONTEXT
from stair_context_inputs import PROFILES
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import INPUT, layout
from stair_junction_solve import solve
from stair_network_experiment import cameras as network_cameras
from stair_network_experiment import outer_ends as network_ends
from stair_network_geometry import interactions, vertex_links
from stair_network_inputs import FIXTURE as NETWORK
from stair_network_inputs import load as network_inputs
from stair_outer_context import catalog
from stair_outer_geometry import bound_contacts, outer_ends
from stair_outer_inputs import FIXTURE, load


def alternatives(args):
    result = []
    for name, options in (
        ("previous_two_knot", {"folded": False}),
        ("early_fold_return", {"return_fraction": 0.5}),
    ):
        _case, rows, nodes, stairs, _context, _recipe = load(args.fixture, **options)
        plan = layout(rows, "width", junctions=nodes, stairs=stairs)
        heights, note = solve(plan)
        if heights is not None or "infeasible" not in note["status"].lower():
            raise ValueError(
                "frozen actual countercase did not reproduce its finite conflict"
            )
        _h, capacity = solve(plan, gap_m=None)
        if not capacity["valid"]:
            raise ValueError("finite conflict capacity check did not finish")
        result.append({"recipe": name, "solve": note, "capacity": capacity})
    for overshoot in (6, 12, 18):
        _case, rows, nodes, stairs, _context, recipe = load(
            args.fixture, overshoot_m=overshoot
        )
        plan = layout(rows, "width", junctions=nodes, stairs=stairs)
        heights, note = solve(plan)
        record = {"recipe": "folded", "overshoot_m": overshoot, "solve": note}
        result.append(record)
        if heights is None:
            continue
        body, data, geometry = build(plan, heights)
        pairs, _whole = interactions(rows, nodes)
        audit(
            data, plan["domains"], limits=[r["gradient"] for r in rows], crossings=pairs
        )
        bound_contacts(data, rows)
        vertex_links(body.vertices, body.triangles)
        record["accepted_geometry"] = geometry
        if overshoot != recipe["continuation_ramp"]["overshoot_m"]:
            raise ValueError(
                "chosen input skipped the first valid bounded folded approach"
            )
        return result
    raise ValueError("bounded folded approaches failed whole-neighbour construction")


def cameras(args, vertices):
    target = (vertices.min(axis=0) + vertices.max(axis=0)) / 2
    eye = target + [600, -800, 650]
    forward = (target - eye) / np.linalg.norm(target - eye)
    right = np.cross(forward, [0, 0, 1])
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    span = 2.2 * max(
        abs((vertices - target) @ right).max(),
        abs((vertices - target) @ up).max() * 1280 / 720,
    )
    for phase in ("before", "after"):
        network_cameras(args, phase)
        for view in ("overview", "diagnostic", "west"):
            path = args.output / phase / (view + "-scene.json")
            row = json.loads(path.read_text())
            row["id"] = "Wien-stair-outer-" + phase + "-" + view
            if view == "overview":
                row.update(
                    eye=eye.tolist(),
                    target=target.tolist(),
                    span_m=float(span),
                    detail_view=False,
                )
            path.write_text(json.dumps(row, indent=2) + "\n")


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
        raise ValueError(
            "fresh outer-neighbour output and clean frozen research required"
        )
    variants = alternatives(args)
    _case, original, old_nodes, old_stairs, _context, _network = network_inputs()
    case, rows, nodes, stairs, context, _recipe = load(args.fixture)
    args.output.mkdir(parents=True)
    before = stage(
        args, "before", original, old_nodes, network_ends(original), old_stairs
    )
    after = stage(args, "after", rows, nodes, outer_ends(rows, nodes), stairs)
    pairs, whole = interactions(rows, nodes)
    if pairs != after["crossings"]:
        raise ValueError("whole band interactions differ from saved audited crossings")
    with np.load(args.output / "after/body.npz") as saved:
        data = dict(saved)
    whole.update(vertex_links(data["body_vertices"], data["body_faces"]))
    after["whole_interactions"] = whole
    after["bound_former_profiles"] = bound_contacts(data, rows)
    cameras(args, data["body_vertices"])
    source_catalog = catalog(case, rows, context)
    catalog_path = args.output / "source-catalog.json"
    catalog_path.write_text(json.dumps(source_catalog, indent=2) + "\n")
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "fixture_sha256": digest(args.fixture),
        "context_sha256": digest(CONTEXT),
        "network_sha256": digest(NETWORK),
        "native_case_sha256": digest(INPUT),
        "profile_recipe_sha256": digest(PROFILES),
        "source_catalog_sha256": digest(catalog_path),
        "before": before,
        "after": after,
        "alternatives": variants,
        "candidate_source_ids": [r["source_id"] for r in rows],
        "admitted_source_ids": [],
        "changed_source_ids": [rows[6]["source_id"]],
        "planned_sources": [
            {
                "source_id": r["source_id"],
                "xy_m": r["xy"].tolist(),
                "prior_m": r["prior"].tolist(),
            }
            for r in rows
        ],
        "unadmitted_line_parts": len(source_catalog["unadmitted_sources"]),
        "unadmitted_area_features": len(source_catalog["unadmitted_non_line_features"]),
        "seconds": time.perf_counter() - start,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "status": "actual_neighbour_profiles_bound; remaining_context_terrain_and_five_outer_profiles_not_admitted",
        "scope": "Whole selected18-source bodies and two actual former-port caps. Full original payload inventory preserves every unplanned line part/area; no surrounding source, terrain, cut/fill, swept-route or native recovery admission.",
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
    parser.add_argument(
        "--palette-scene",
        type=Path,
        default=Path(__file__).with_name("fixtures") / "research-palette.json",
    )
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    generate(arguments())
