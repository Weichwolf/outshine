"""Compare the isolated recipe with actual neighbours and both real overpasses."""

import argparse
import itertools
import json
import resource
import subprocess
import time
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import shapely
from camera_height_inputs import digest
from stair_context_inputs import FIXTURE, PROFILES, load
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_display import display
from stair_junction_experiment import outer_ports
from stair_junction_layout import INPUT, inputs, layout
from stair_junction_solve import solve


def crossings(rows, junctions=None):
    result = []
    for i, j in itertools.combinations(range(len(rows)), 2):
        a, b = rows[i], rows[j]
        parts = shapely.get_parts(
            shapely.LineString(a["xy"]).intersection(shapely.LineString(b["xy"]))
        )
        for point in parts:
            if point.geom_type != "Point":
                continue
            p = np.array(point.coords[0])
            if junctions is not None and any(
                i in node["incident"]
                and j in node["incident"]
                and np.linalg.norm(p - node["xy"]) < 1e-7
                for node in junctions
            ):
                continue
            if all(
                np.min(np.linalg.norm(row["xy"][[0, -1]] - p, axis=1)) < 1e-7
                for row in (a, b)
            ):
                continue
            if a["original"]["bridge"] == b["original"]["bridge"]:
                raise ValueError("unplanned crossing between equal bridge kinds")
            low, high = (j, i) if a["original"]["bridge"] else (i, j)
            result.append([low, high, p.tolist()])
    return result


def frontier_ends(fixture, rows):
    result = []
    keys = fixture["selected_source_ids"]
    for record in fixture["frontier"]:
        row = record["selected"]
        i = keys.index(row["source_id"])
        at = row["vertex"]
        if at not in (0, len(rows[i]["xy"]) - 1):
            raise ValueError("actual frontier is not at a full source endpoint")
        result.append((i, 0 if at == 0 else -1))
    return result


def stage(args, name, rows, junctions, ends=None, stairs=None):
    plan = layout(rows, "width", junctions=junctions, stairs=stairs)
    heights, note = solve(plan)
    if heights is None:
        raise ValueError("bounded contextual candidate failed: " + note["status"])
    body, data, geometry = build(plan, heights)
    pairs = crossings(rows, junctions)
    proof = audit(
        data, plan["domains"], limits=[r["gradient"] for r in rows], crossings=pairs
    )
    output = args.output / name
    output.mkdir()
    np.savez_compressed(output / "body.npz", **data)
    np.savez_compressed(
        output / "height-input.npz",
        xy=plan["xy"],
        heights=heights,
        **{"faces" + str(i): f for i, f in enumerate(plan["parts"])},
    )
    presentation = display(
        SimpleNamespace(output=output, palette_scene=args.palette_scene), body, data
    )
    for view in ("overview", "diagnostic"):
        path = output / (view + "-scene.json")
        row = json.loads(path.read_text())
        row["id"] = "Wien-stair-context-" + name + "-" + view
        if view == "diagnostic":
            row.update(eye=[240, 235, 195], target=[277, 185, 166], span_m=110)
        path.write_text(json.dumps(row, indent=2) + "\n")
    return {
        "body_sha256": digest(output / "body.npz"),
        "height_input_sha256": digest(output / "height-input.npz"),
        "geometry": geometry,
        "physical": proof,
        "solve": note,
        "presentation": presentation,
        "source_ids": [r["source_id"] for r in rows],
        "limits": [r["gradient"] for r in rows],
        "crossings": pairs,
        "proposed_outer_profiles": outer_ports(plan, heights, ends),
    }


def generate(args):
    began = time.perf_counter()
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
        raise ValueError("fresh context output and frozen clean research required")
    _case, rows, junctions, fixture = load(args.fixture, args.native_case)
    _case, original = inputs(args.native_case)
    args.output.mkdir(parents=True)
    before = stage(args, "before", original, None)
    after = stage(args, "after", rows, junctions, frontier_ends(fixture, rows))
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "native_case_sha256": digest(args.native_case),
        "fixture_sha256": digest(args.fixture),
        "profile_recipe_sha256": digest(PROFILES),
        "before": before,
        "after": after,
        "candidate_source_ids": fixture["selected_source_ids"],
        "admitted_source_ids": [],
        "unadmitted_context_source_ids": [
            r["source_id"]
            for r in fixture["sources"]
            if r["role"] == "context_not_admitted"
        ],
        "unadmitted_reason": "outside_current_candidate_field; requires source-level/contact/clearance planning before admission",
        "frontier": fixture["frontier"],
        "seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "status": "candidate_requires_complete_context_clearance_and_outer_contacts",
        "scope": "Both local body models are independently auditable. Extra known context and actual outer products remain unverified; neither this candidate nor a native/world/terrain-identity claim is admitted.",
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
    parser.add_argument("--native-case", type=Path, default=INPUT)
    parser.add_argument("--palette-scene", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    generate(arguments())
