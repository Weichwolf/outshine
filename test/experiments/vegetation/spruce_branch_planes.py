"""Compare existing radial shoots with explicitly parent-referenced branch fans."""

import argparse
import copy
import json
from pathlib import Path

import numpy as np
from curve_frame import arc_frame
from parent_profile_graph import ParentProfileGraph
from spruce_growth_probe import cached_areas, measure
from spruce_study_source import INPUT, read_input
from spruce_whorl_balance import balanced


def branch_form(original, *, planar=False, fixed=False, span=1.0):
    if (
        type(planar) is not bool
        or type(fixed) is not bool
        or fixed
        and not planar
        or type(span) not in (int, float)
        or not np.isfinite(span)
        or not 1 <= span <= 1.4
    ):
        raise ValueError("explicit branch fan and bounded1..1.4main span required")
    recipe = copy.deepcopy(original)
    main, fine = recipe["nodes"][2:4]
    if planar:
        fine["placement"]["pattern"]["divergence"] = float(np.pi)
        fine["placement"]["azimuth_variance"] = 0.18
    if fixed:
        fine["placement"]["phase"] = {
            "version": "fixed-parent-azimuth-1",
            "radians": 0.0,
        }
    if span != 1:
        main["shape"]["length"] *= span
        spacing = fine["placement"]["pattern"]["spacing"]
        spacing["table"] = (np.array(spacing["table"]) * span).tolist()
        recipe["parent_length_ratios"] = [
            [key, ratio / span if key == 3 else ratio]
            for key, ratio in recipe["parent_length_ratios"]
        ]
    return recipe


def lines(pose):
    centre = arc_frame(pose.length, pose.bend, 0, np.linspace(0, 1, 17))[0]
    return centre @ pose.frame.T + pose.origin


def sparse_poses(recipe):
    graph = ParentProfileGraph.from_recipe(recipe)
    main = graph.instances(2, 242)
    if main is None or len(main) != 240:
        raise ValueError("same complete240main branches required")
    branches = []
    for slot in (60, 120, 180, 239):
        parent = main[slot]
        sites = graph.site_table(3, parent)
        branches.append(
            (
                parent,
                [graph.instance(parent.path + ((3, i),)) for i in range(len(sites))],
            )
        )
    points = np.vstack([lines(p) for p in main])
    return (
        main,
        branches,
        {
            "probe_prepared_poses": len(graph._poses),
            "primary_centreline_bounds_m": [
                points.min(0).tolist(),
                points.max(0).tolist(),
            ],
            "primary_xy_span_m": np.ptp(points[:, :2], axis=0).tolist(),
            "main_max_length_m": max(p.length for p in main),
            "scope": "17centreline samples per main axis and four selected complete secondary fans; no fine whole plant or needles, not a certified continuous crown bound.",
        },
    )


def probe(source, output):
    data, base = read_input(INPUT)
    original = balanced(base, 48, 1.6)
    metadata = json.loads((source / "catalog/catalog.json").read_text())
    areas, hashes = cached_areas(source / "models")
    output.mkdir(parents=True, exist_ok=False)
    rows = []
    for name, options in (
        ("radial", {}),
        ("rotated-fan", {"planar": True}),
        ("lateral-fan", {"planar": True, "fixed": True}),
        ("wider-lateral-fan", {"planar": True, "fixed": True, "span": 1.25}),
    ):
        recipe = branch_form(original, **options)
        settings = data["authoring"] | {
            "primary_whorls": 48,
            "secondary_spacing_factor": 1.6 * options.get("span", 1),
            "crown_span_factor": options.get("span", 1),
            "secondary_divergence_rad": recipe["nodes"][3]["placement"]["pattern"][
                "divergence"
            ],
            "secondary_phase": recipe["nodes"][3]["placement"].get("phase"),
        }
        result = measure(recipe, data["physical_source"], metadata, areas)
        _, _, spatial = sparse_poses(recipe)
        error = abs(result["expected_area_m2"] / settings["target_area_m2"] - 1)
        row = {
            "name": name,
            **result,
            **spatial,
            "relative_area_error": error,
            "admitted": error <= 0.05,
        }
        if error > 0.05:
            raise ValueError("same nominal area target and unchanged5%bound required")
        rows.append(row)
        (output / f"{name}.json").write_text(json.dumps(recipe, indent=2) + "\n")
        (output / f"{name}-authoring.json").write_text(
            json.dumps(settings, indent=2) + "\n"
        )
        print(json.dumps(row), flush=True)
    (output / "preflight.json").write_text(json.dumps(rows, indent=2) + "\n")
    (output / "area-inputs.json").write_text(json.dumps(hashes, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    probe(args.source, args.output)
