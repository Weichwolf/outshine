"""Matched physical shoot controls for statistical windows, outside the showcase."""

import argparse
import json
import time
from pathlib import Path

import numpy as np
from conifer_whole_products import BASE, OUTPUT, SPECIES, load_catalog
from conifer_window_catalog import template_image
from conifer_window_render import tree_bounds, window_fragments
from conifer_windows import plan_windows
from foliage_selection import directions
from leaf_sheet import average_blocks
from needle_group_source import source_geometry
from outshine_ref.vegetation import NeedledPlant
from plant_atlas import projection
from textured_raster import composite, wood_fragments


def shoot_sources(species, whole):
    directory = BASE / species
    source = source_geometry(directory)
    recipe = json.loads((directory / "whole-plan.json").read_text())
    plant = NeedledPlant.from_recipe(recipe)
    catalog, banks = load_catalog(whole / species / "catalog")
    windows = plan_windows(plant, catalog)
    axis = plant.wood.axis(tuple(source["axis_path"]))
    selected = windows["axis_ids"] == axis.id
    windows = {key: value[selected] for key, value in windows.items()}
    windows["origins"] = (windows["origins"] - axis.origin) @ axis.frame
    windows["frames"] = np.einsum("ji,njk->nik", axis.frame, windows["frames"])
    source["leaf"] = np.array(
        [source["ids"][int(i)][0] == "leaf" for i in source["organ_faces"]]
    )
    main_axis = source["organ_faces"] == 0
    wood = {
        "vertices": source["vertices"],
        "faces": source["faces"][main_axis],
        "normals": source["normals"][main_axis],
    }
    bounds = np.concatenate((source["vertices"], tree_bounds(windows, banks, wood)))
    return source, windows, banks, wood, bounds


def compare_one(species, whole, size=128):
    began = time.perf_counter()
    source, windows, banks, wood, bounds = shoot_sources(species, whole)
    rows = []
    for direction in directions(17):
        projection_data = projection(bounds, direction)
        frame, lower, upper, _ = projection_data
        physical, _ = template_image(
            source, direction, size, 4, projection_data=projection_data
        )
        parts = (
            window_fragments(windows, banks, direction, frame, lower, upper, size * 4),
            wood_fragments(wood, frame, lower, upper, size * 4),
        )
        pixel, depth, data = (np.concatenate([p[i] for p in parts]) for i in range(3))
        coarse = average_blocks(composite(pixel, depth, data, size * 4), 4)
        a, b = physical[..., 3], coarse[..., 3]
        area = max(float(a.sum()), 1e-20)
        rows.append(
            {
                "direction": direction.tolist(),
                "physical_covered_pixels": float(a.sum()),
                "window_covered_pixels": float(b.sum()),
                "relative_area_error": float(abs(b.sum() - a.sum()) / area),
                "normalized_coverage_l1": float(abs(b - a).sum() / area),
                "coverage_l1_by_view_size": {
                    str(size // factor): float(
                        abs(average_blocks(b, factor) - average_blocks(a, factor)).sum()
                        / max(float(average_blocks(a, factor).sum()), 1e-20)
                    )
                    for factor in (1, 4, 16)
                },
            }
        )
    return {
        "species": species,
        "axis_path": source["axis_path"],
        "source_needles": sum(kind == "leaf" for kind, _ in source["ids"]),
        "window_needles": int(windows["needle_counts"].sum()),
        "windows": len(windows["origins"]),
        "template_families": len(banks),
        "cpu_elapsed_s": time.perf_counter() - began,
        "directions": rows,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--whole", type=Path, default=OUTPUT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    rows = []
    for species in SPECIES:
        row = compare_one(species, args.whole)
        rows.append(row)
        print(
            species,
            "max relative area error",
            max(r["relative_area_error"] for r in row["directions"]),
            flush=True,
        )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(
            {
                "scope": "one cached physical shoot per species; same wood axis, fixed shared bounds, 17 unlit orthographic directions, 128 square pixels and 4x4 subpixel sampling; statistical/template/tail errors are measured, not approved",
                "species": rows,
            },
            indent=2,
        )
        + "\n"
    )
