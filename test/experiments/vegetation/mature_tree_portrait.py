"""Matched lit portraits of complete cached mature forms; no plant or atlas build."""

import argparse
import hashlib
import json
import resource
import time
from pathlib import Path
from unittest.mock import patch

import bpy
import gpu
import numpy as np
from conifer_preview import setup_scene_lights
from conifer_whole_preview import add_trees
from outshine_ref.vegetation import Needle, NeedledPlant, NeedlePattern, PlantPlan
from preview_shadows import configure_preview

SCENE = {
    "camera": [18, -70, 24],
    "target": [0, 0, 16.6],
    "fov_y": 30,
    "ground_size_m": 100,
    "clip_end_m": 250,
    "fill_position_m": [-30, -40, 55],
    "fill_target_m": [0, 0, 17],
    "fill_energy_w": 40000,
    "fill_size_m": 30,
    "samples": 32,
    "resolution": [2560, 1440],
}


def load_product(inputs):
    start = time.perf_counter()
    manifest = json.loads((inputs / "plant-product.json").read_text())
    recipe = json.loads((inputs / manifest["recipe"]).read_text())
    if (
        manifest["version"] != "whole-plant-views-1"
        or hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
        != manifest["plan_identity"]
    ):
        raise ValueError(
            "Complete versioned cached plant identity must match its recipe"
        )
    names = [
        "plant-product.json",
        manifest["recipe"],
        manifest["mesh"],
        *manifest["textures"].values(),
    ]
    hashes = {
        name: hashlib.sha256((inputs / name).read_bytes()).hexdigest() for name in names
    }
    return manifest, {
        "files": hashes,
        "file_bytes": sum((inputs / n).stat().st_size for n in names),
        "header_and_checksums_s": time.perf_counter() - start,
    }


def render(inputs, output):
    started = time.perf_counter()
    output.mkdir(parents=True, exist_ok=False)
    manifest, source = load_product(inputs)
    # The legacy cache reader expects a species directory and a small scene index.
    # Both refer to the unchanged complete product, never a regenerated plant.
    aliases = output / "products"
    aliases.mkdir()
    (aliases / "picea-abies").symlink_to(inputs.resolve(), target_is_directory=True)
    (output / "portrait-plans.json").write_text(
        json.dumps(
            {
                "plants": [
                    {
                        "species": {"latin": "Picea abies"},
                        "identity": manifest["plan_identity"],
                        "origin_m": [0, 0, 0],
                        "frame": np.eye(3).tolist(),
                    }
                ]
            }
        )
    )
    scene_recipe = SCENE | {"plant_recipe": "portrait-plans.json"}
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if bpy.context.scene.world is None:
        bpy.context.scene.world = bpy.data.worlds.new("portrait world")
    with (
        patch.object(
            NeedledPlant, "foliage", side_effect=AssertionError("organs on hit")
        ),
        patch.object(
            NeedlePattern, "detail", side_effect=AssertionError("placement on hit")
        ),
        patch.object(Needle, "mesh", side_effect=AssertionError("meshing on hit")),
        patch.object(PlantPlan, "axis", side_effect=AssertionError("axes on hit")),
    ):
        _paths, triangles = add_trees(scene_recipe, output, aliases)
    bound = time.perf_counter()
    setup_scene_lights(scene_recipe)
    scene = bpy.context.scene
    configure_preview(scene)
    scene.eevee.taa_render_samples = SCENE["samples"]
    scene.render.resolution_x, scene.render.resolution_y = SCENE["resolution"]
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str((output / "portrait.png").resolve())
    ready = time.perf_counter()
    print(
        json.dumps(
            {
                "stage": "render",
                "identity": manifest["plan_identity"],
                "bind_s": bound - started,
                "triangles": triangles,
            }
        ),
        flush=True,
    )
    bpy.ops.render.render(write_still=True)
    report = {
        "identity": manifest["plan_identity"],
        "source": source,
        "scene": SCENE,
        "bind_s": bound - started,
        "environment_s": ready - bound,
        "render_s": time.perf_counter() - ready,
        "process_s": time.perf_counter() - started,
        "process_peak_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "draw_triangles_all_camera_light_roles": triangles,
        "selected_views": [
            obj.name for obj in scene.objects if obj.get("vegetation_pass_role")
        ],
        "backend": gpu.platform.backend_type_get(),
        "renderer": gpu.platform.renderer_get(),
        "scope": "Same whole flat lit portrait for existing complete cached forms. Root pose/needle/atlas construction forbidden on hit. Statistical windows, mean normals and material response remain unapproved; no native/device/LOD/SpeedTree admission.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    render(args.inputs, args.output)
