"""Complete source ranges, actual near attachments and fresh cached stand queries."""

import argparse
import json
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from mature_forest_plan import initial_views, load_plan, select_views
from outshine_ref.vegetation import Needle, NeedledPlant, NeedlePattern, PlantPlan
from outshine_ref.vegetation.curve_frame import arc_frame
from PIL import Image
from preview_lighting import LANDSCAPE_SUN_ROTATION
from scipy.spatial.transform import Rotation


def check_near_ranges(products_path):
    rows = []
    for case in json.loads(products_path.read_text()):
        directory = products_path.parent / case["product"]
        recipe = json.loads((directory / "plant-recipe.json").read_text())
        plant = NeedledPlant.from_recipe(recipe)
        with np.load(directory / "windows.npz", allow_pickle=False) as stored:
            ids = stored["first_needle_ids"]
            counts, owners = stored["needle_counts"], stored["axis_ids"]
        assert int(ids[0]) == 0
        np.testing.assert_array_equal(ids[1:], ids[:-1] + counts[:-1].astype(np.uint64))
        assert int(ids[-1]) + int(counts[-1]) == sum(plant.needle_counts)
        assert len(ids) == case["summary"]["windows"]
        paths = [
            tuple(0 for _ in plant.wood.levels[1:]),
            tuple(level.branches // 2 for level in plant.wood.levels[1:]),
            tuple(level.branches - 1 for level in plant.wood.levels[1:]),
        ]
        error = 0.0
        for path in paths:
            detail = plant.foliage(path, 100000, retain=False)
            axis = plant.wood.axis(path)
            selected = owners == axis.id
            assert int(ids[selected].min()) == int(detail["ids"][0])
            assert int(counts[selected].sum()) == len(detail["ids"])
            parameters = detail["parent_parameters"]
            centre = (
                arc_frame(axis.length, axis.bend, 0, parameters)[0] @ axis.frame.T
                + axis.origin
            )
            measured = np.linalg.norm(detail["short_origins"] - centre, axis=1)
            base, tip = plant.wood.radii(path)
            expected = base * (1 - parameters) + tip * parameters
            error = max(error, float(abs(measured - expected).max()))
            np.testing.assert_allclose(measured, expected, atol=2e-12, rtol=0)
        assert not plant._groups
        rows.append(
            {
                "key": case["key"],
                "windows": len(ids),
                "needles": sum(plant.needle_counts),
                "near_subsets": len(paths),
                "max_radial_offset_error_m": error,
            }
        )
    return rows


def expected_views(recipe, sites, models, views):
    sun = Rotation.from_euler("xyz", LANDSCAPE_SUN_ROTATION).apply((0, 0, 1))
    checked = 0
    for model, queries in zip(models, views, strict=True):
        pivot = np.mean(model["manifest"]["bounds_m"], axis=0)
        for local_index, index in enumerate(model["sites"]):
            angle = float(sites["angles"][index])
            c, s = np.cos(angle), np.sin(angle)
            frame = np.array(((c, -s, 0), (s, c, 0), (0, 0, 1)))
            centre = sites["positions"][index] + frame @ pivot
            for role, world in (
                ("camera", np.asarray(recipe["camera"]) - centre),
                ("sun", sun),
            ):
                ray = np.linalg.solve(frame, world)
                expected = max(
                    range(len(model["mesh"]["directions"])),
                    key=lambda i: np.dot(ray, model["mesh"]["directions"][i]),
                )
                assert int(queries[role][local_index]) == expected
                checked += 1
    return checked


def cached_stand(source, products):
    with (
        patch.object(PlantPlan, "axis", side_effect=AssertionError("cache axis")),
        patch.object(
            NeedledPlant, "foliage", side_effect=AssertionError("cache organs")
        ),
        patch.object(
            NeedledPlant, "windows", side_effect=AssertionError("cache windows")
        ),
        patch.object(
            NeedlePattern, "detail", side_effect=AssertionError("cache pattern")
        ),
        patch.object(Needle, "mesh", side_effect=AssertionError("cache mesh")),
    ):
        began = time.perf_counter()
        recipe, sites, models, inputs = load_plan(source, products)
        maps = []
        for model in models:
            fields = {}
            for key, filename in model["manifest"]["textures"].items():
                with Image.open(model["directory"] / filename) as image:
                    assert list(image.size) == model["manifest"]["atlas_resolution"]
                    fields[key] = np.array(image)
            assert np.all(fields["leaf_weight"] <= fields["colour"][..., 3])
            np.testing.assert_array_equal(
                fields["normal"][..., 3], fields["colour"][..., 3]
            )
            maps.append(fields)
        decode_s = time.perf_counter() - began
        arrays = (
            tuple(sites.values())
            + tuple(a for model in models for a in model["mesh"].values())
            + tuple(a for fields in maps for a in fields.values())
        )
        allocation_ids = tuple(id(a) for a in arrays)
        for array in arrays:
            array.setflags(write=False)
        views = initial_views(recipe, sites, models)
        probes = expected_views(recipe, sites, models, views)
        original = source.parent / recipe["sites"]
        with np.load(original, allow_pickle=False) as saved:
            for key in saved.files:
                np.testing.assert_array_equal(sites[key], saved[key])
        target = np.asarray(recipe["target"])
        offset = np.asarray(recipe["camera"]) - target
        times, selected = [], []
        for angle in np.linspace(0, 2 * np.pi, 60, endpoint=False):
            c, s = np.cos(angle), np.sin(angle)
            eye = target + np.array(
                (
                    c * offset[0] - s * offset[1],
                    s * offset[0] + c * offset[1],
                    offset[2],
                )
            )
            began = time.perf_counter_ns()
            row = [select_views(model, sites, eye=eye) for model in models]
            times.append((time.perf_counter_ns() - began) / 1e6)
            selected.append(row)
        current_arrays = (
            tuple(sites.values())
            + tuple(a for model in models for a in model["mesh"].values())
            + tuple(a for fields in maps for a in fields.values())
        )
        assert allocation_ids == tuple(id(a) for a in current_arrays)
    return {
        "scope": "fresh Python CPU cache decode and 60 cached view queries; all original stand sites/IDs retained; no generation/upload, OS file caches may be warm; no native frame/GPU/A18 gate",
        "complete_models": len(models),
        "stand_sites": len(sites["ids"]),
        "species_counts": recipe["counts"],
        "decode_s": decode_s,
        "resident_array_bytes": sum(a.nbytes for a in arrays),
        "frames": len(times),
        "cpu_query_ms_p50_p95_p99": np.percentile(times, (50, 95, 99)).tolist(),
        "independent_camera_sun_probes": probes,
        "generation_disabled": True,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = {
        "near": check_near_ranges(args.products),
        "cache": cached_stand(args.scenario, args.products),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
