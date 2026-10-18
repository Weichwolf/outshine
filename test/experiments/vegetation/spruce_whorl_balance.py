"""Bounded main-whorl/fine-shoot allocation at the same physical foliage target."""

import argparse
import copy
import json
from pathlib import Path

import numpy as np
from spruce_growth_probe import cached_areas, internode_profile, measure
from spruce_study_source import INPUT, read_input


def balanced(original, whorls, secondary_spacing_factor):
    if (
        type(whorls) is not int
        or whorls not in (36, 40, 44, 48)
        or not np.isfinite(secondary_spacing_factor)
        or not 1 <= secondary_spacing_factor <= 2
    ):
        raise ValueError(
            "declared36..48complete whorls and bounded fine spacing required"
        )
    recipe = copy.deepcopy(original)
    primary = recipe["nodes"][2]["placement"]
    primary["pattern"]["limit_groups"] = whorls
    primary["pattern"]["spacing"]["table"] = internode_profile(
        recipe["nodes"][1]["shape"]["length"],
        primary["first"],
        primary["last"],
        whorls,
        recipe["seed"],
        0.35,
        upper_bias=0.5,
    )
    fine = recipe["nodes"][3]["placement"]["pattern"]["spacing"]
    fine["table"] = (np.array(fine["table"]) * secondary_spacing_factor).tolist()
    return recipe


def probe(input_path, source, output):
    data, original = read_input(input_path)
    metadata = json.loads((source / "catalog/catalog.json").read_text())
    areas, area_inputs = cached_areas(source / "models")
    output.mkdir(parents=True, exist_ok=False)
    cases = [(36, 1.0)] + [
        (n, scale) for n in (40, 44, 48) for scale in (1.15, 1.25, 1.35, 1.45, 1.55)
    ]
    cases += [
        (n, factor)
        for n, values in (
            (40, (1.26, 1.27, 1.28)),
            (44, (1.46, 1.47, 1.48)),
            (48, (1.58, 1.59, 1.60)),
        )
        for factor in values
    ]
    rows = []
    for whorls, spacing in cases:
        name = f"whorls-{whorls}-spacing-{spacing}"
        graph = balanced(original, whorls, spacing)
        settings = data["authoring"] | {
            "primary_whorls": whorls,
            "secondary_spacing_factor": spacing,
        }
        row = {
            "name": name,
            "primary_whorls": whorls,
            "secondary_spacing_factor": spacing,
        }
        try:
            result, lengths = measure(
                graph, data["physical_source"], metadata, areas, include_lengths=True
            )
            fine = graph["nodes"][3]["placement"]
            spacing_m = fine["pattern"]["spacing"]["table"][0]
            desired = (
                np.floor(
                    (fine["last"] - fine["first"]) * lengths[1] / spacing_m
                ).astype(int)
                + 1
            )
            result["capped_primary_axes"] = int(
                (desired > fine["pattern"]["limit_groups"]).sum()
            )
            row |= result
            error = (
                abs(result["expected_area_m2"] - settings["target_area_m2"])
                / settings["target_area_m2"]
            )
            row |= {"relative_area_error": error, "admitted": error <= 0.05}
            settings |= result
        except ValueError as error:
            row["rejected"] = str(error)
        (output / f"{name}.json").write_text(json.dumps(graph, indent=2) + "\n")
        (output / f"{name}-authoring.json").write_text(
            json.dumps(settings, indent=2) + "\n"
        )
        rows.append(row)
        print(json.dumps(row), flush=True)
    (output / "preflight.json").write_text(json.dumps(rows, indent=2) + "\n")
    (output / "area-inputs.json").write_text(json.dumps(area_inputs, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=INPUT)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    probe(args.input, args.source, args.output)
