"""Small seeded graminoid form products from the existing analytical Blade."""

import argparse
import hashlib
import json
import time
from dataclasses import asdict, replace
from pathlib import Path

import numpy as np
from outshine_ref.vegetation.blade_surface import Blade
from scipy.spatial.transform import Rotation

GRASS = Blade(
    length=0.45,
    width=0.009,
    widest=0.1,
    lobes=0,
    lobe_depth=0,
    base_fill=0.7,
    bend=1.15,
    fold=0.25,
    fold_power=1,
    twist=0,
    petiole=0,
)


def tuft(seed, blades=12, segments=8):
    if (
        type(seed) is not int
        or not 0 <= seed < 2**64
        or type(blades) is not int
        or not 1 <= blades <= 64
    ):
        raise ValueError("uint64 seed and one to64 authored tuft blades required")
    rng = np.random.Generator(np.random.PCG64(seed))
    vertices, faces, normals, uv, blade_ids = [], [], [], [], []
    offset = 0
    for blade_index in range(blades):
        blade = replace(
            GRASS,
            length=rng.uniform(0.25, 0.48),
            width=rng.uniform(0.006, 0.012),
            bend=rng.uniform(0.6, 1.4),
            twist=rng.uniform(-0.2, 0.2),
        )
        mesh = blade.mesh(segments, across=1)
        # Uppercase intrinsic axes give world-Z azimuth after upright lean.
        frame = Rotation.from_euler(
            "ZX", (rng.uniform(0, 2 * np.pi), np.pi / 2 - rng.uniform(0.08, 0.4))
        ).as_matrix()
        angle, radius = rng.uniform(0, 2 * np.pi), 0.06 * np.sqrt(rng.random())
        origin = np.array((radius * np.cos(angle), radius * np.sin(angle), -0.008))
        vertices.append(mesh["vertices"] @ frame.T + origin)
        blade_ids.append(np.full(len(mesh["vertices"]), blade_index, dtype=np.uint8))
        faces.append(mesh["faces"] + offset)
        normals.append(mesh["normals"] @ frame.T)
        uv.append(
            np.column_stack(
                ((mesh["parameters"][:, 1] + 1) / 2, mesh["parameters"][:, 0])
            )
        )
        offset += len(mesh["vertices"])
    return {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces),
        "normals": np.concatenate(normals),
        "uv": np.concatenate(uv),
        "blade_index": np.concatenate(blade_ids),
    }


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
