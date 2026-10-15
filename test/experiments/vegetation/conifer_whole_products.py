"""Build finite shoot catalogs, then complete whole trees without needle generation."""

import argparse
import json
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from conifer_window_catalog import bake_template, catalog_sources
from conifer_window_render import plan_wood, tree_bounds, tree_image
from conifer_windows import plan_windows
from outshine_ref.vegetation import Needle, NeedledPlant, NeedlePattern
from plant_atlas import bake_product

BASE = Path("build/experiments/vegetation/conifer-crowns")
OUTPUT = Path("build/experiments/vegetation/conifer-whole")
SPECIES = ("picea-abies", "pinus-sylvestris", "pinus-cembra", "larix-decidua")


def save_catalog(directory, families, banks):
    directory.mkdir(parents=True, exist_ok=True)
    metadata = {}
    for key, (_, info) in families.items():
        bank = banks[info["family"]]
        np.savez_compressed(
            directory / f"family-{info['family']}.npz",
            images=np.array([v[0] for v in bank]),
            frames=np.array([v[1][0] for v in bank]),
            lower=np.array([v[1][1] for v in bank]),
            upper=np.array([v[1][2] for v in bank]),
            offsets=np.array([v[1][3] for v in bank]),
        )
        metadata[key] = info
    (directory / "catalog.json").write_text(json.dumps(metadata, indent=2) + "\n")


def load_catalog(directory):
    metadata = json.loads((directory / "catalog.json").read_text())
    banks = [None] * len(metadata)
    for info in metadata.values():
        with np.load(
            directory / f"family-{info['family']}.npz", allow_pickle=False
        ) as f:
            images, frames, lower, upper, offsets = (
                f[n] for n in ("images", "frames", "lower", "upper", "offsets")
            )
        for values in (images, frames, lower, upper, offsets):
            values.setflags(write=False)
        banks[info["family"]] = [
            (image, (frame, lo, hi, off))
            for image, frame, lo, hi, off in zip(
                images, frames, lower, upper, offsets, strict=True
            )
        ]
    return metadata, banks


def prepare_catalog(species, output_root=OUTPUT):
    output = output_root / species
    began = time.perf_counter()
    plant, families, inputs = catalog_sources(BASE / species)
    banks = [bake_template(source) for source, _ in families.values()]
    save_catalog(output / "catalog", families, banks)
    catalog_s = time.perf_counter() - began
    source = {
        "recipe": plant.recipe(),
        "input_sha256": inputs,
        "catalog_build_s": catalog_s,
        "scope": "finite canonical physical templates; no whole-plant needle expansion",
    }
    (output / "catalog/source.json").write_text(json.dumps(source, indent=2) + "\n")
    return source


def build_one(species, output_root=OUTPUT, size=128):
    output = output_root / species
    provenance = json.loads((output / "catalog/source.json").read_text())
    recipe = provenance["recipe"]
    with (
        patch.object(
            NeedledPlant,
            "foliage",
            side_effect=AssertionError("coarse needle placement"),
        ),
        patch.object(
            NeedlePattern, "detail", side_effect=AssertionError("coarse organ detail")
        ),
        patch.object(
            Needle, "mesh", side_effect=AssertionError("coarse needle meshing")
        ),
    ):
        plant = NeedledPlant.from_recipe(recipe)
        began = time.perf_counter()
        catalog, banks = load_catalog(output / "catalog")
        catalog_load_s = time.perf_counter() - began
        began = time.perf_counter()
        windows = plan_windows(plant, catalog)
        if windows is None:
            raise ValueError("whole product window budget exceeded")
        wood = plan_wood(plant)
        preparation_s = time.perf_counter() - began
        np.savez_compressed(output / "windows.npz", **windows)
        source = {
            "recipe": recipe,
            "plan_identity": plant.identity,
            "input_sha256": provenance["input_sha256"],
            "bounds_points": tree_bounds(windows, banks, wood),
            "source_scope": "axis plan and finite physical window catalog; all individual-needle placement and meshing disabled during whole build; statistical template/tail/shading errors remain open",
        }
        result = bake_product(
            source,
            output,
            size=size,
            draw=lambda d, size: tree_image(windows, banks, wood, d, size, samples=1),
        )
        assert not plant._groups
    result.update(
        species=species,
        window_count=len(windows["origins"]),
        needle_count=int(windows["needle_counts"].sum()),
        catalog_build_s=provenance["catalog_build_s"],
        catalog_load_s=catalog_load_s,
        coarse_preparation_s=preparation_s,
        template_families=len(catalog),
        window_array_bytes=sum(a.nbytes for a in windows.values()),
        wood_triangles=len(wood["faces"]),
        needle_generation_disabled=True,
    )
    (output / "whole-build.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        species,
        result["window_count"],
        result["needle_count"],
        result["generation_ms"],
        flush=True,
    )
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--size", type=int, choices=(64, 128, 256), default=128)
    parser.add_argument("--phase", choices=("catalog", "build", "all"), default="all")
    args = parser.parse_args()
    for name in SPECIES:
        if args.phase in ("catalog", "all"):
            prepare_catalog(name, args.output)
        if args.phase in ("build", "all"):
            build_one(name, args.output, args.size)
