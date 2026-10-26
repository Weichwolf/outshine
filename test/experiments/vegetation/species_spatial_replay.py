"""Replay saved correlated choices without source/camera/taxonomy work."""

import argparse
import json
import sys
from pathlib import Path
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import SpeciesHeightMixture, species_site_uniforms
from species_spatial import FourierMarks


def replay(directory):
    report = json.loads((directory / "comparison.json").read_text())
    data = np.load(directory / "choices.npz", allow_pickle=False)
    checked = 0
    with (
        patch.object(
            FourierMarks, "prepare", side_effect=AssertionError("field rebuild")
        ),
        patch.object(
            SpeciesHeightMixture,
            "prepare",
            side_effect=AssertionError("weight rebuild"),
        ),
    ):
        field = FourierMarks.from_recipe(report["field"])
        for subset in (
            np.arange(len(data["ids"]) - 1, -1, -1),
            np.arange(0, len(data["ids"]), 11),
        ):
            u = species_site_uniforms(data["ids"][subset], report["site_seed"])
            u = field.uniforms(data["world_m"][subset], u, report["strength"])
            np.testing.assert_array_equal(u, data["uniforms"][subset])
            for i, entry in enumerate(report["mixtures"]):
                active = data["mixture_index"][subset] == i
                mixture = SpeciesHeightMixture.from_recipe(entry["mixture"])
                np.testing.assert_array_equal(
                    mixture.choose(data["height_m"][subset][active], u[active]),
                    data["species_index"][subset][active],
                )
                checked += int(active.sum())
    forbidden = (
        "outshine_ref.places",
        "outshine_ref.vegetation.species_catalog",
        "pyproj",
        "bpy",
        "species_spatial_experiment",
    )
    imported = sorted(
        m
        for m in sys.modules
        if any(m == f or m.startswith(f + ".") for f in forbidden)
    )
    if imported:
        raise AssertionError(f"replay imported preparation dependencies: {imported}")
    result = {
        "exact_replayed_forest_choices": checked,
        "all_variates_exact": True,
        "source_provider_catalog_or_generator_imported": False,
        "prepared_field_or_weights_rebuilt": False,
    }
    (directory / "replay.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--product", type=Path, required=True)
    replay(p.parse_args().product)
