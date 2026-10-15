"""Fresh whole-product decode and cached CPU view selection; no generation/upload."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from conifer_whole_products import OUTPUT, SPECIES
from forest_views import choices
from outshine_ref.vegetation import Needle, NeedledPlant, NeedlePattern, PlantPlan
from PIL import Image


def decode(directory):
    manifest = json.loads((directory / "plant-product.json").read_text())
    recipe = json.loads((directory / manifest["recipe"]).read_text())
    identity = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    assert identity == manifest["plan_identity"]
    with np.load(directory / manifest["mesh"], allow_pickle=False) as stored:
        mesh = {key: stored[key] for key in stored.files}
    maps = {}
    for name, filename in manifest["textures"].items():
        with Image.open(directory / filename) as image:
            maps[name] = np.array(image)
    arrays = (*mesh.values(), *maps.values())
    for array in arrays:
        array.setflags(write=False)
    return manifest, mesh, maps


def measure(products):
    with (
        patch.object(PlantPlan, "axis", side_effect=AssertionError("cache axes")),
        patch.object(
            NeedledPlant, "foliage", side_effect=AssertionError("cache needles")
        ),
        patch.object(
            NeedlePattern, "detail", side_effect=AssertionError("cache pattern")
        ),
        patch.object(Needle, "mesh", side_effect=AssertionError("cache mesh")),
    ):
        began = time.perf_counter()
        plants = [decode(products / species) for species in SPECIES]
        decode_s = time.perf_counter() - began
        arrays = tuple(
            a for _, mesh, maps in plants for a in (*mesh.values(), *maps.values())
        )
        allocations = tuple(id(a) for a in arrays)
        queries, selected = [], []
        for angle in np.linspace(0, 2 * np.pi, 60, endpoint=False):
            began = time.perf_counter_ns()
            direction = np.array((np.cos(angle), np.sin(angle), 0.35))
            row = [
                int(
                    choices(
                        mesh,
                        np.zeros((1, 3)),
                        np.ones((1, 3)),
                        np.zeros(1),
                        np.mean(manifest["bounds_m"], axis=0),
                        direction=direction,
                    )[0]
                )
                for manifest, mesh, _ in plants
            ]
            queries.append((time.perf_counter_ns() - began) / 1e6)
            selected.append(row)
        assert allocations == tuple(
            id(a) for _, mesh, maps in plants for a in (*mesh.values(), *maps.values())
        )
    return {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "scope": "fresh Python process; all four complete cached products decoded once; 60 CPU-only direction queries, no texture/mesh rebuild or upload; warm OS file cache allowed; not a native/frame/GPU/A18 gate",
        "decode_s": decode_s,
        "resident_array_bytes": sum(a.nbytes for a in arrays),
        "frames": len(queries),
        "query_ms_p50_p95_p99": np.percentile(queries, (50, 95, 99)).tolist(),
        "selected_views_per_species": [
            len(set(row[i] for row in selected)) for i in range(len(plants))
        ],
        "generation_disabled": True,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, default=OUTPUT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = measure(args.products)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
