"""Physical recipe/cache checks and exact preservation of existing plant assets."""

import json
import tempfile
from pathlib import Path

import numpy as np
from europe_conifer_products import OUTPUT, write_json
from europe_conifer_recipes import conifers
from needle_material import write_needle_material
from outshine_ref.vegetation import NeedledPlant
from plant_experiment import sapling
from scipy.spatial import cKDTree


def old_products():
    arrays = 0
    for seed in (0, 17, 381):
        plant = sapling(seed)
        old = json.loads((OUTPUT / f"before-sapling-{seed}.json").read_text())
        assert plant.recipe() == old["recipe"]
        assert json.loads(json.dumps(plant.summary())) == old["summary"]
        with np.load(OUTPUT / f"before-sapling-{seed}.npz") as before:
            tables = plant.tables(100000, 100000)
            assert set(tables) == set(before.files)
            for key in before.files:
                np.testing.assert_array_equal(tables[key], before[key])
                arrays += 1
    maps = 0
    with tempfile.TemporaryDirectory(prefix="outshine-vegetation-needle-maps-") as tmp:
        output = Path(tmp)
        for section, case in (("sector", 0), ("rhombus", 2)):
            write_needle_material(output, section)
            source = OUTPUT.parent / "needle-sectors" / f"case-{case}-near"
            for kind in ("colour", "roughness"):
                filename = f"needle-{section}-{kind}.png"
                assert (output / filename).read_bytes() == (
                    source / filename
                ).read_bytes()
                maps += 1
    return {
        "legacy_seeds": 3,
        "exact_canonical_arrays": arrays,
        "exact_default_needle_pngs": maps,
    }


def check_species(species):
    directory = OUTPUT / species.key
    plan = NeedledPlant.from_recipe(
        json.loads((directory / "whole-plan.json").read_text())
    )
    tables = plan.tables(100000, 100000)
    with np.load(directory / "whole-tables.npz") as cached:
        assert set(tables) == set(cached.files)
        for key in cached.files:
            np.testing.assert_array_equal(cached[key], tables[key])
    near = json.loads((directory / "shoot.json").read_text())
    whole = json.loads((directory / "whole.json").read_text())
    assert near["plan_identity"] == whole["plan_identity"] == plan.identity
    expected = plan.foliage(tuple(near["selected_axis_path"]), 100000)
    seen = []
    nested_error = 0
    for product in near["products"]:
        if product["surface"] != "leaf":
            continue
        with np.load(directory / product["placements"]) as poses:
            mask = np.isin(expected["ids"], poses["ids"])
            for field in ("ids", "origins", "frames", "scales"):
                np.testing.assert_array_equal(poses[field], expected[field][mask])
            seen.extend(poses["ids"])
        stem = Path(product["mesh"]).name.replace("-shoot-", "-whole-")
        with (
            np.load(directory / stem) as coarse,
            np.load(directory / product["mesh"]) as fine,
        ):
            distance = cKDTree(fine["vertices"]).query(coarse["vertices"])[0]
            nested_error = max(nested_error, float(distance.max()))
    assert nested_error < 2e-17
    np.testing.assert_array_equal(np.sort(seen), expected["ids"])
    length = (
        np.array([n.length for n in plan.needles])[tables["foliage_variants"]]
        * tables["foliage_scales"]
    )
    ranges = {
        "picea-abies": (0.01, 0.025),
        "pinus-sylvestris": (0.04, 0.08),
        "pinus-cembra": (0.05, 0.09),
        "larix-decidua": (0.015, 0.04),
    }
    low, high = ranges[species.key]
    assert (length >= low).all() and (length <= high).all()
    return {
        "species": species.key,
        **plan.summary(),
        "near_axis_path": near["selected_axis_path"],
        "near_needles": len(seen),
        "needle_length_m": [float(length.min()), float(length.max())],
        "nested_prototype_vertex_error_m": nested_error,
        "cache_arrays_exact": len(tables),
    }


def check_tile():
    directory = OUTPUT / "tile"
    plans = json.loads((directory / "tile-plans.json").read_text())["plants"]
    recipe = json.loads((directory / "tile.json").read_text())
    products = [p for p in recipe["products"] if "plant_id" in p]
    errors = []
    for product in products:
        plan = plans[product["plant_id"]]
        source = OUTPUT / plan["species"]["latin"].lower().replace(" ", "-")
        pose = Path(product["placements"]).name.removeprefix("tile-")
        with (
            np.load(directory / product["placements"]) as cached,
            np.load(source / pose) as original,
        ):
            np.testing.assert_array_equal(cached["ids"], original["ids"])
            np.testing.assert_array_equal(cached["scales"], original["scales"])
            restored = (cached["origins"] - plan["origin_m"]) @ np.asarray(
                plan["frame"]
            )
            errors.append(float(np.max(abs(restored - original["origins"]))))
    assert max(errors) < 3e-15
    return {
        "plants": len(plans),
        "tree_products": len(products),
        "max_rigid_pose_recovery_error_m": max(errors),
    }


def main():
    report = {
        "legacy": old_products(),
        "species": [check_species(s) for s in conifers()],
        "tile": check_tile(),
    }
    write_json(OUTPUT / "checks.json", report)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
