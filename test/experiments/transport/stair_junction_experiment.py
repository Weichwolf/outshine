"""Fresh actual four-way stair recipe with explicit unbound outer boundary products."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_display import display
from stair_junction_layout import INPUT, inputs, layout
from stair_junction_solve import solve


def outer_ports(plan, heights):
    result = []
    for owner, end in ((1, -1), (2, 0)):
        row = plan["rows"][owner]
        centre = row["xy"][end]
        tangent = row["xy"][1] - centre if end == 0 else centre - row["xy"][-2]
        tangent /= np.linalg.norm(tangent)
        normal = np.array([-tangent[1], tangent[0]])
        selected = np.unique(plan["parts"][owner])
        delta = plan["xy"][selected] - centre
        on = (abs(delta @ tangent) < 1e-7) & (
            np.linalg.norm(delta, axis=1) <= row["width_m"]
        )
        selected = selected[on]
        selected = selected[np.argsort((plan["xy"][selected] - centre) @ normal)]
        if len(selected) < 2:
            raise ValueError("full proposed outer port is absent")
        result.append(
            {
                "source_id": row["source_id"],
                "native_lane_index": row["original"]["native_lane_index"],
                "source_endpoint_prior_m": float(row["prior"][end]),
                "proposed_vertices_enu_m": np.c_[
                    plan["xy"][selected], heights[selected]
                ].tolist(),
                "height_change_range_m": [
                    float((heights[selected] - row["prior"][end]).min()),
                    float((heights[selected] - row["prior"][end]).max()),
                ],
                "status": "proposed_pending_actual_neighbour_contact",
            }
        )
    return result


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
        raise ValueError("fresh output and frozen clean four-way research required")
    case, rows = inputs(args.input)
    comparisons = {}
    fixed = layout(rows, "fixed")
    _none, comparisons["fixed_core"] = solve(
        fixed, gap_m=case["minimum_crossing_gap_m"] + 0.02
    )
    plan = layout(rows, "width")
    heights, comparisons["width_core"] = solve(
        plan, gap_m=case["minimum_crossing_gap_m"] + 0.02
    )
    _none, comparisons["fixed_outer_priors"] = solve(
        plan, fixed_port_heights=True, gap_m=case["minimum_crossing_gap_m"] + 0.02
    )
    if heights is None:
        raise ValueError("bounded local width-aware stair recipe was not solved")
    body, data, geometry = build(plan, heights)
    proof = audit(data, plan["domains"])
    args.output.mkdir(parents=True)
    target = args.output / "body.npz"
    np.savez_compressed(target, **data)
    field = {
        "xy": plan["xy"],
        "heights": heights,
        **{"faces" + str(i): f for i, f in enumerate(plan["parts"])},
    }
    np.savez_compressed(args.output / "height-input.npz", **field)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "input_sha256": digest(args.input),
        "body_sha256": digest(target),
        "height_input_sha256": digest(args.output / "height-input.npz"),
        "source_ids": [r["source_id"] for r in rows],
        "comparisons": comparisons,
        "layout": plan["note"],
        "geometry": geometry,
        "physical": proof,
        "outer_ports": outer_ports(plan, heights),
        "presentation": display(args, body, data),
        "seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "status": "valid_local_recipe_pending_actual_outer_contacts",
        "scope": "Actual four source footprints and local recipe; input elevations remain priors. No whole Vienna, native/DEM/terrain-cut, complete walking route, structural or consumer-release claim.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "commit",
                    "seconds",
                    "peak_rss_kib",
                    "geometry",
                    "outer_ports",
                    "status",
                )
            }
        ),
        flush=True,
    )


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=INPUT)
    parser.add_argument("--palette-scene", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (60, 60))
    generate(arguments())
