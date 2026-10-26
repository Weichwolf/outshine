"""Verify cached choices with fresh reversed/subset requests and no GIS/source work."""

import argparse
import json
import sys
from pathlib import Path

import numpy as np
from species_mixture import HeightMixture, site_uniforms


def verify(directory):
    report = json.loads((directory / "comparison.json").read_text())
    data = np.load(directory / "choices.npz", allow_pickle=False)
    queries = (
        np.arange(len(data["ids"]) - 1, -1, -1),
        np.arange(0, len(data["ids"]), 11),
    )
    checked = 0
    for indices in queries:
        for group, entry in enumerate(report["mixtures"]):
            selected = indices[data["mixture_index"][indices] == group]
            mixture = HeightMixture.from_recipe(entry["mixture"])
            actual = mixture.choose(
                data["height_m"][selected],
                site_uniforms(data["ids"][selected], report["site_seed"]),
            )
            np.testing.assert_array_equal(actual, data["species_index"][selected])
            checked += len(selected)
    unsupported = data["mixture_index"] < 0
    np.testing.assert_array_equal(data["species_index"][unsupported], -1)
    forbidden = (
        "outshine_ref.places",
        "outshine_ref.vegetation.species_catalog",
        "pyproj",
        "bpy",
        "place_camera",
        "species_mixture_experiment",
    )
    if any(
        name == p or name.startswith(p + ".") for name in sys.modules for p in forbidden
    ):
        raise AssertionError("replay imported source/GIS/generator machinery")
    answer = {
        "sites": len(data["ids"]),
        "checked_reversed_and_subset_draws": checked,
        "source_provider_catalog_or_generator_imported": False,
        "base_geometry_dependency_imported": "shapely" in sys.modules,
        "prepared_weight_source_needed": False,
        "all_choices_exact": True,
        "scope": "Stored height/species plan only; no geometry/visual/native cache admission",
    }
    (directory / "replay.json").write_text(json.dumps(answer, indent=2) + "\n")
    print(json.dumps(answer), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--product", type=Path, required=True)
    verify(parser.parse_args().product)
