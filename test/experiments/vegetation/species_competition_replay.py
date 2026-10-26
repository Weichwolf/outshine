"""Replay competing species fields without provider/taxonomy or field rebuild."""

import argparse
import json
import sys
from pathlib import Path
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import SpeciesHeightMixture
from species_competition import CompetingMarks
from species_spatial import FourierMarks


def replay(directory):
    report = json.loads((directory / "comparison.json").read_text())
    data = np.load(directory / "choices.npz", allow_pickle=False)
    checked = 0
    with (
        patch.object(CompetingMarks, "prepare", side_effect=AssertionError("rebuild")),
        patch.object(
            FourierMarks, "prepare", side_effect=AssertionError("field rebuild")
        ),
        patch.object(
            SpeciesHeightMixture,
            "prepare",
            side_effect=AssertionError("weight rebuild"),
        ),
    ):
        marks = CompetingMarks.from_recipe(report["competing_marks"])
        for subset in (
            np.arange(len(data["ids"]) - 1, -1, -1),
            np.arange(0, len(data["ids"]), 11),
        ):
            for group, entry in enumerate(report["mixtures"]):
                indices = subset[data["mixture_index"][subset] == group]
                mixture = SpeciesHeightMixture.from_recipe(entry["mixture"])
                actual = marks.choose(
                    mixture,
                    data["world_m"][indices],
                    data["ids"][indices],
                    data["height_m"][indices],
                    report["strength"],
                )
                np.testing.assert_array_equal(actual, data["species_index"][indices])
                checked += len(indices)
    forbidden = (
        "outshine_ref.places",
        "outshine_ref.vegetation.species_catalog",
        "pyproj",
        "bpy",
        "species_competition_experiment",
        "species_spatial_experiment",
    )
    imported = [
        m
        for m in sys.modules
        if any(m == f or m.startswith(f + ".") for f in forbidden)
    ]
    if imported:
        raise AssertionError(f"preparation imported: {imported}")
    result = {
        "exact_replayed_forest_choices": checked,
        "field_or_weight_preparation_repeated": False,
        "provider_catalog_or_generator_imported": False,
    }
    (directory / "replay.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--product", type=Path, required=True)
    replay(p.parse_args().product)
