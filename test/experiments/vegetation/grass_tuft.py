"""Small seeded graminoid form products from the existing analytical Blade."""

import argparse
import hashlib
import json
import time
from dataclasses import asdict
from pathlib import Path

import numpy as np
from outshine_ref.vegetation.graminoid import (
    GRAMINOID_BLADE as GRASS,
)
from outshine_ref.vegetation.graminoid import (
    graminoid_tuft_mesh as tuft,
)


def export(output, seed=17, variants=8):
    if type(variants) is not int or not 1 <= variants <= 16:
        raise ValueError("one to16 finite shared tuft variants required")
    began = time.perf_counter()
    recipe = {
        "format": "graminoid-tuft-plan-1",
        "seed": seed,
        "variants": variants,
        "blades": 12,
        "segments": 8,
        "base_blade": asdict(GRASS),
        "scope": "Authored unidentified graminoid form, not a calibrated or accepted botanical species",
    }
    identity = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    parts = [tuft(seed + i) for i in range(variants)]
    v, t = len(parts[0]["vertices"]), len(parts[0]["faces"])
    if any(len(p["vertices"]) != v or len(p["faces"]) != t for p in parts):
        raise ValueError("matching variant topology required")
    mesh = {
        key: np.concatenate(
            [p[key] + (i * v if key == "faces" else 0) for i, p in enumerate(parts)]
        )
        for key in parts[0]
    }
    bounds = np.array((mesh["vertices"].min(0), mesh["vertices"].max(0))).tolist()
    manifest = {
        "format": "graminoid-tuft-product-1",
        "representation": "spatial_mesh",
        "plan_identity": identity,
        "recipe": "tuft-recipe.json",
        "mesh": "tufts.npz",
        "variants": variants,
        "vertices_per_variant": v,
        "triangles_per_variant": t,
        "bounds_m": bounds,
        "render_bounds_m": bounds,
        "textures": {},
        "scope": recipe["scope"],
    }
    output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(output / manifest["mesh"], **mesh)
    (output / manifest["recipe"]).write_text(json.dumps(recipe, indent=2) + "\n")
    (output / "plant-product.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(
        json.dumps(
            {
                "elapsed_s": time.perf_counter() - began,
                "variants": variants,
                "triangles_per_variant": t,
                "bounds_m": bounds,
                "identity": identity,
            }
        )
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output", type=Path, required=True)
    export(p.parse_args().output)
