"""Inspect all cached graminoid form variants without organ reconstruction."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import bpy
import gpu
import numpy as np
from organ_preview import setup_scene
from outshine_ref.vegetation import species_site_uniforms
from outshine_ref.vegetation.blade_surface import Blade
from place_grass import populate_grass
from preview_shadows import configure_preview


def render(product, scenario_path, output):
    began = time.perf_counter()
    recipe = json.loads(scenario_path.read_text())
    manifest = json.loads((product / "plant-product.json").read_text())
    planned = json.loads((product / manifest["recipe"]).read_text())
    identity = hashlib.sha256(json.dumps(planned, sort_keys=True).encode()).hexdigest()
    if identity != manifest["plan_identity"]:
        raise ValueError("cached form identity changed")
    with np.load(product / manifest["mesh"], allow_pickle=False) as encoded:
        mesh = dict(encoded)
    count = manifest["variants"]
    candidates = np.arange(1024, dtype=np.uint64)[:, None]
    variant = (species_site_uniforms(candidates, 9417) * count).astype(int)
    keys = candidates[[np.flatnonzero(variant == i)[0] for i in range(count)]]
    sites = {
        "ids": keys,
        "positions": np.array([(i % 4 * 0.6, i // 4 * 0.6, 0) for i in range(count)]),
        "frames": np.repeat(np.eye(3)[None], count, axis=0),
        "scales": np.ones(count),
    }
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    with patch.object(
        Blade, "mesh", side_effect=AssertionError("cached inspection rebuilt organs")
    ):
        triangles = populate_grass(
            manifest,
            identity,
            mesh,
            sites,
            np.arange(count),
            {"camera": np.ones(count, bool), "sun": np.ones(count, bool)},
        )
        setup_scene(recipe)
        scene = bpy.context.scene
        configure_preview(scene)
        scene.render.resolution_x, scene.render.resolution_y = recipe["profile"]
        scene.render.resolution_percentage = 100
        scene.eevee.taa_render_samples = recipe["samples"]
        scene.render.image_settings.file_format = "PNG"
        scene.view_settings.view_transform = "AgX"
        output.mkdir(parents=True, exist_ok=True)
        target = output / "graminoid-variants.png"
        scene.render.filepath = str(target.resolve())
        bpy.ops.render.render(write_still=True)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "product_identity": identity,
        "scenario_sha256": hashlib.sha256(scenario_path.read_bytes()).hexdigest(),
        "profile": recipe["profile"],
        "samples": recipe["samples"],
        "variants": count,
        "triangles": triangles,
        "source": "authored near control; not geographic plant observations",
        "organ_reconstruction_forbidden": True,
        "renderer": gpu.platform.renderer_get(),
        "backend": gpu.platform.backend_type_get(),
        "elapsed_s": time.perf_counter() - began,
        "image_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
        "scope": recipe["scope"],
    }
    (output / "graminoid-variants.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--product", type=Path, required=True)
    p.add_argument("--scenario", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    render(a.product, a.scenario, a.output)
