"""Prepare two camera-independent complete growth sources from the skeleton pilot."""

import argparse
import hashlib
import json
import shutil
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from compact_crown import CompactCrown
from crown_shoots import CrownShoots
from limb_experiment import canonical_wood
from oak_scaffold import tapered_radii
from oak_skin_experiment import seed_continuations
from shoot_form import ShootForm
from spatial_group_source import load_source


def prepare_case(pilot, original, output, name):
    began = time.perf_counter()
    result = json.loads((pilot / "report.json").read_text())
    metrics, recipe = result["products"][name], result["recipe"]
    folder = output / name
    spatial = folder / "spatial"
    spatial.mkdir(parents=True)
    with np.load(pilot / f"{name}.npz", allow_pickle=False) as z:
        positions, parents = z["positions"], z["parents"]
    for filename in (
        "tables.npz",
        "fern-material.json",
        "fern-colour.png",
        "fern-height.png",
    ):
        shutil.copyfile(original / "spatial" / filename, spatial / filename)
    np.savez_compressed(spatial / "skeleton.npz", positions=positions, parents=parents)
    metadata = {
        "recipe": recipe,
        "products": {
            "spatial": {
                "identity": metrics["identity"],
                "source_seed_nodes": metrics["source_seed_nodes"],
            }
        },
        "experiment": "subcrown-space-1",
        "placement": name,
        "source_pilot_sha256": hashlib.sha256(
            (pilot / f"{name}.npz").read_bytes()
        ).hexdigest(),
    }
    (folder / "comparison.json").write_text(json.dumps(metadata, indent=2) + "\n")
    source = load_source(folder)
    if source.crown.identity != metrics["identity"]:
        raise AssertionError("source plan changed")
    # No source leaf pose/group or near mesh may be created during this stage.
    with (
        patch.object(CrownShoots, "group", side_effect=AssertionError),
        patch("outshine_ref.vegetation.Shoot.detail", side_effect=AssertionError),
        patch("outshine_ref.vegetation.Blade.mesh", side_effect=AssertionError),
    ):
        base = CompactCrown.build(source)
        form = ShootForm.build(source, base)
        form.save(folder / "form.npz")
        radii = tapered_radii(
            parents, recipe["root_radius_m"], recipe["terminal_radius_m"]
        )
        hints = seed_continuations(parents, parents[: metrics["source_seed_nodes"]])
        built = canonical_wood(
            source.crown, radii=radii, max_nodes=16384, continuations=hints
        )
    if built is None:
        raise ValueError("complete C1 wood exceeds declared capacity")
    wood, paths = built
    np.savez_compressed(folder / "wood.npz", **wood)
    summary = {
        "source_identity": source.identity,
        "plan_identity": source.crown.identity,
        "form_identity": form.identity,
        "hosts": len(form.scale),
        "wood_triangles": len(wood["faces"]),
        "wood_paths": len(paths),
        "longest_wood_path_stations": max(map(len, paths)),
        "wood_array_bytes": sum(v.nbytes for v in wood.values()),
        "compact_array_bytes": sum(v.nbytes for v in base.data.values()),
        "leaf_variants": np.bincount(source.crown.variants, minlength=8).tolist(),
        "analytic_blade_area_m2": sum(
            b.area() * n
            for b, n in zip(
                source.blades, np.bincount(source.crown.variants), strict=True
            )
        ),
        "prepare_s": time.perf_counter() - began,
    }
    (folder / "product.json").write_text(json.dumps(summary, indent=2) + "\n")
    return summary


def run(pilot, original, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    results = {
        name: prepare_case(pilot, original, output, name)
        for name in ("original", "limb")
    }
    a, b = results.values()
    if (
        a["leaf_variants"] != b["leaf_variants"]
        or a["analytic_blade_area_m2"] != b["analytic_blade_area_m2"]
    ):
        raise AssertionError("physical source inventory changed")
    (output / "report.json").write_text(json.dumps(results, indent=2) + "\n")
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pilot", type=Path, required=True)
    parser.add_argument("--original", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.pilot, args.original, args.output)
