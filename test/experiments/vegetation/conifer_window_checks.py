"""Budgets, identity ranges and physical frames without a full near expansion."""

import argparse
import json
import subprocess
import types
from pathlib import Path
from unittest.mock import patch

import numpy as np
from conifer_whole_products import BASE, OUTPUT, SPECIES, load_catalog
from conifer_windows import plan_windows, window_count
from forest_views import choices
from outshine_ref.vegetation import Needle, NeedledPlant, NeedlePattern, PlantPlan
from PIL import Image
from textured_raster import source_image


def raster_compatibility():
    previous = types.ModuleType("previous_textured_raster")
    exec(
        subprocess.check_output(
            ["git", "show", "434290b50:test/experiments/vegetation/textured_raster.py"],
            text=True,
        ),
        previous.__dict__,
    )
    quad = np.array(((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)), float)
    faces = np.array(((0, 1, 2), (0, 2, 3)))
    normals = np.tile((0, 0, 1), (6, 1))
    mesh = {
        "vertices": quad,
        "faces": faces,
        "normals": normals,
        "uv": np.array(((0, 0), (1, 0), (1, 1), (0, 1))),
    }
    colour = np.broadcast_to((0.5, 0, 0, 0.5), (8, 8, 4)).copy()
    normal = np.broadcast_to((0.25, 0.25, 0.5, 0.5), (8, 8, 4)).copy()
    wood = {"vertices": quad - (0, 0, 0.5), "faces": faces, "normals": normals}
    args = (mesh, (colour, normal), wood, np.eye(3), (-1, -1), (1, 1), 8)
    before = previous.source_image(*args)
    after = source_image(*args)
    np.testing.assert_array_equal(before, after)
    expected = np.array((0.57, 0.0525, 0.0185, 1, 0, 0, 1, 0.5, 0.75))
    np.testing.assert_allclose(
        after, np.broadcast_to(expected, after.shape), atol=1e-12
    )


def check(species, whole=OUTPUT):
    recipe = json.loads((BASE / species / "whole-plan.json").read_text())
    catalog, banks = load_catalog(whole / species / "catalog")
    plant = NeedledPlant.from_recipe(recipe)
    expected = window_count(plant, catalog)
    with patch.object(
        PlantPlan, "axis", side_effect=AssertionError("budget expanded axes")
    ):
        assert plan_windows(plant, catalog, max_windows=expected - 1) is None
        assert plan_windows(plant, catalog, max_axes=0) is None
    assert not plant.wood._axes
    with (
        patch.object(
            NeedledPlant, "foliage", side_effect=AssertionError("coarse needles")
        ),
        patch.object(
            NeedlePattern, "detail", side_effect=AssertionError("coarse pattern")
        ),
        patch.object(Needle, "mesh", side_effect=AssertionError("coarse meshes")),
    ):
        windows = plan_windows(plant, catalog)
    assert len(windows["origins"]) == expected and not plant._groups
    order = np.argsort(windows["first_needle_ids"])
    ids = windows["first_needle_ids"][order]
    counts = windows["needle_counts"][order]
    assert ids[0] == 0
    np.testing.assert_array_equal(ids[1:], ids[:-1] + counts[:-1].astype(np.uint64))
    assert int(ids[-1]) + int(counts[-1]) == sum(plant.needle_counts)
    assert np.all((windows["weights"] > 0) & (windows["weights"] <= 1))
    for info in catalog.values():
        selected = windows["families"] == info["family"]
        np.testing.assert_allclose(
            windows["weights"][selected] * info["needles"],
            windows["needle_counts"][selected],
            atol=1e-14,
        )
    np.testing.assert_allclose(
        np.einsum("nji,njk->nik", windows["frames"], windows["frames"]),
        np.broadcast_to(np.eye(3), windows["frames"].shape),
        atol=4e-14,
    )
    assert np.all(np.linalg.det(windows["frames"]) > 0.999999999999)
    for bank in banks:
        for image, (frame, lower, upper, offset) in bank:
            assert image.shape == (32, 32, 9) and not image.flags.writeable
            assert (
                np.isfinite(image).all()
                and np.all(image[..., 3] >= 0)
                and np.all(image[..., 3] <= 1)
            )
            assert np.all(image[..., 7] <= image[..., 3] + 1e-7)
    paths = list(plant.wood.paths())
    chosen = [p for p in paths if plant.slot_counts[len(p)][p[-1] if p else 0]]
    for path in (chosen[0], chosen[len(chosen) // 2], chosen[-1]):
        near = NeedledPlant.from_recipe(recipe)
        detail = near.foliage(path, 100000)
        selected = windows["axis_ids"] == near.wood.axis(path).id
        assert int(windows["first_needle_ids"][selected].min()) == int(detail["ids"][0])
        assert int(windows["needle_counts"][selected].sum()) == len(detail["ids"])
    return {
        "species": species,
        "windows": expected,
        "needles": sum(plant.needle_counts),
        "families": len(catalog),
    }


def check_product(species, whole):
    directory = whole / species
    product = json.loads((directory / "plant-product.json").read_text())
    recipe = json.loads((directory / product["recipe"]).read_text())
    plant = NeedledPlant.from_recipe(recipe)
    assert product["plan_identity"] == plant.identity
    with np.load(directory / product["mesh"], allow_pickle=False) as stored:
        mesh = {key: stored[key] for key in stored.files}
    assert len(mesh["directions"]) == product["views"] == 64
    assert np.isfinite(mesh["vertices"]).all()
    assert mesh["faces"].min() >= 0 and mesh["faces"].max() < len(mesh["vertices"])
    bounds = np.array(product["bounds_m"])
    pivot = bounds.mean(axis=0)
    selected = []
    with (
        patch.object(PlantPlan, "axis", side_effect=AssertionError("cache axes")),
        patch.object(
            NeedledPlant, "foliage", side_effect=AssertionError("cache organs")
        ),
        patch.object(
            NeedlePattern, "detail", side_effect=AssertionError("cache pattern")
        ),
        patch.object(Needle, "mesh", side_effect=AssertionError("cache mesh")),
    ):
        for angle in np.linspace(0, 2 * np.pi, 60, endpoint=False):
            direction = np.array((np.cos(angle), np.sin(angle), 0.35))
            index = choices(
                mesh,
                np.zeros((1, 3)),
                np.ones((1, 3)),
                np.zeros(1),
                pivot,
                direction=direction,
            )
            selected.append(int(index[0]))
        maps = {}
        for name, filename in product["textures"].items():
            with Image.open(directory / filename) as image:
                assert list(image.size) == product["atlas_resolution"]
                maps[name] = np.asarray(image).copy()
    assert len(set(selected)) > 8 and not plant.wood._axes and not plant._groups
    assert np.all(maps["leaf_weight"] <= maps["colour"][..., 3])
    np.testing.assert_array_equal(maps["colour"][..., 3], maps["normal"][..., 3])
    return {"species": species, "cache_orbit_views": len(set(selected)), "organs": 0}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--whole", type=Path, default=OUTPUT)
    parser.add_argument("--products", action="store_true")
    args = parser.parse_args()
    raster_compatibility()
    for name in SPECIES:
        print(check(name, args.whole), flush=True)
        if args.products:
            print(check_product(name, args.whole), flush=True)
