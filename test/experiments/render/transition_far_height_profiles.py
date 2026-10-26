"""Full-cell views immediately across a source's metadata-only LOD threshold."""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from compare_far_height_profiles import (
    combine,
    comparison_views,
    cook_selected,
    plan_cell,
    selected_profiles,
    subset,
    write_scene,
)
from full_view_reference import load


def run(args):
    source, document, context, _ = load(args.input)
    plans, needs, surfaces, _, _, _, _ = plan_cell(args, document)
    named, centre, right, up = comparison_views(source, document, context)
    metadata = {s["index"]: s for s in document["sources"]}
    direction = named["static-020"][0] - centre
    direction /= np.linalg.norm(direction)
    forward = -direction
    camera_right = np.cross(forward, up)
    camera_right /= np.linalg.norm(camera_right)
    camera = np.stack((camera_right, np.cross(camera_right, forward), forward))

    def at_distance(distance):
        return centre + distance * direction, camera, *named["static-020"][2:]

    def admitted(distance):
        return (
            args.source
            in selected_profiles(
                {args.source: plans[args.source]},
                surfaces,
                metadata,
                at_distance(distance),
            )[0]
        )

    low, high = 20.0, 50000.0
    if admitted(low) or not admitted(high):
        raise ValueError(
            "Threshold must be bracketed independently of a rendered image"
        )
    for _ in range(50):
        middle = (low + high) / 2
        if admitted(middle):
            high = middle
        else:
            low = middle
    threshold = (low + high) / 2
    poses = {
        name: at_distance(threshold + delta)
        for name, delta in [("before", -0.125), ("after", 0.125)]
    }
    choices = {
        name: selected_profiles(plans, surfaces, metadata, view)
        for name, view in poses.items()
    }
    if args.source in choices["before"][0] or args.source not in choices["after"][0]:
        raise ValueError("Expected real source representation switch is missing")
    required = set().union(*(selected for selected, _ in choices.values()))
    generated, _ = cook_selected(plans, required, needs)
    basis = np.stack((right, np.cross(up, right), up))
    local = (source["positions"].astype(float) - centre) @ basis.T
    ground = {
        "z": float(local[:, 2].min() - 1),
        "span": float(np.ptp(local[:, :2], axis=0).max() * 4),
    }
    args.output.mkdir(parents=True, exist_ok=False)
    producer = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    original = subset(source, np.ones(len(source["indices"]), bool))
    rows = []
    held_candidate = None
    for name, view in poses.items():
        selected, limits = choices[name]
        selected &= set(generated)
        fallback = subset(source, ~np.isin(source["binding"][:, 1], list(selected)))
        candidate = combine([fallback, *(generated[i] for i in sorted(selected))])
        if set(candidate["source_id"]) != set(original["source_id"]):
            raise ValueError("Incomplete cell at transition")
        if name == "before":
            held_candidate = candidate
        for strategy, mesh in [("original", original), ("profile", candidate)]:
            write_scene(
                args.output / name / strategy,
                mesh,
                view,
                basis,
                centre,
                ground,
                producer,
            )
        rows.append(
            {
                "pose": name,
                "selected_sources": sorted(selected),
                "target_bound_px": limits[args.source],
                "triangles": len(candidate["indices"]),
            }
        )
    before_set, after_set = (choices[name][0] for name in ["before", "after"])
    if after_set - before_set != {args.source} or before_set - after_set:
        raise ValueError("Fixed-eye control requires exactly one changed source")
    write_scene(
        args.output / "after-hold/profile",
        held_candidate,
        poses["after"],
        basis,
        centre,
        ground,
        producer,
    )
    report = {
        "producer": producer,
        "source": args.source,
        "threshold_distance_m": threshold,
        "eye_separation_m": 0.25,
        "fixed_eye_control": "after-hold/profile uses after camera with before selection; only target source remains original",
        "views": rows,
        "needs": needs,
        "scope": "Full original cell, fixed orientation and intrinsics, two eyes 0.25m apart bracketing a genuine source LOD switch. Raw hard-switch baseline; no temporal/radiance/caster certificate.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "views"}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--source", type=int, required=True)
    run(parser.parse_args())
