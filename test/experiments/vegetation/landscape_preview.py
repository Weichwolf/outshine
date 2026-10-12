"""Render a declared landscape form study with shared prototype meshes."""

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).parent))
from blender_preview import foliage_material, material, mesh_object
from forest_preview import populate_views
from landscape_experiment import ground_height
from organ_preview import link_shadow_products
from preview_lighting import LANDSCAPE_SUN_ROTATION
from preview_shadows import configure_preview


def terrain(recipe):
    size = recipe["size_m"] * 3
    side = 128
    xy = np.linspace(-size / 2, size / 2, side)
    x, y = np.meshgrid(xy, xy)
    vertices = np.column_stack((x.ravel(), y.ravel(), ground_height(x, y).ravel()))
    indices = np.arange(side * side).reshape(side, side)
    a, b, c, d = (
        indices[:-1, :-1],
        indices[:-1, 1:],
        indices[1:, 1:],
        indices[1:, :-1],
    )
    faces = np.stack(
        (np.stack((a, b, c), axis=-1), np.stack((a, c, d), axis=-1)), axis=2
    ).reshape(-1, 3)
    normals = np.column_stack(
        (
            -0.036 * np.cos(0.03 * x.ravel()) * np.cos(0.025 * y.ravel()),
            0.03 * np.sin(0.03 * x.ravel()) * np.sin(0.025 * y.ravel()),
            np.ones(x.size),
        )
    )
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    mesh_object(
        "ground", vertices, faces, normals, material("ground", recipe["ground"], 0.95)
    )


def populate(recipe, directory):
    sites_path = directory / recipe["sites"]
    with np.load(sites_path, allow_pickle=False) as stored:
        sites = {key: stored[key] for key in stored.files}
    if any(product["part"] == "plant_view" for product in recipe["products"]):
        sources, triangles, objects = populate_views(recipe, directory, sites)
        return [sites_path, *sources], triangles, objects
    foliage = foliage_material()
    if recipe["architecture"] == "conifer":
        ramp = next(
            node
            for node in foliage.node_tree.nodes
            if node.bl_idname == "ShaderNodeValToRGB"
        )
        ramp.color_ramp.elements[0].color = (0.012, 0.045, 0.025, 1)
        ramp.color_ramp.elements[1].color = (0.065, 0.135, 0.055, 1)
    wood = material("wood", (0.16, 0.1, 0.055), 0.85)
    sources = [directory / recipe["sites"]]
    triangles, objects = 0, 0
    for product in recipe["products"]:
        mask = (sites["kinds"] == product["kind"]) & (
            sites["variants"] == product["variant"]
        )
        positions = sites["positions"][mask]
        path = directory / product["mesh"]
        sources.append(path)
        if not len(positions):
            continue
        with np.load(path) as mesh:
            original = mesh_object(
                path.stem,
                mesh["vertices"],
                mesh["faces"],
                mesh["normals"],
                foliage if product["part"] == "crown" else wood,
                corner_normals=bool(mesh["corner_normals"]),
            )
        original.location = positions[0]
        for position in positions[1:]:
            instance = original.copy()
            instance.data = original.data
            instance.location = position
            bpy.context.collection.objects.link(instance)
        triangles += product["triangles"] * len(positions)
        objects += len(positions)
    return sources, triangles, objects


def setup_scene(recipe):
    terrain(recipe)
    bpy.ops.object.light_add(type="SUN", location=(0, 0, 60))
    bpy.context.object.rotation_euler = recipe.get(
        "sun_rotation", LANDSCAPE_SUN_ROTATION
    )
    bpy.context.object.data.energy = 3
    bpy.context.object.data.angle = 0.06
    link_shadow_products(bpy.context.object, "sun")
    world = bpy.context.scene.world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (
        0.55,
        0.68,
        0.82,
        1,
    )
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.6
    bpy.ops.object.camera_add(location=recipe["camera"])
    camera = bpy.context.object
    camera.rotation_euler = (
        (Vector(recipe["target"]) - camera.location).to_track_quat("-Z", "Y").to_euler()
    )
    camera.data.sensor_fit = "VERTICAL"
    camera.data.sensor_height = 24
    camera.data.lens = 12 / np.tan(np.deg2rad(recipe["fov_y"]) / 2)
    camera.data.clip_end = 2000
    bpy.context.scene.camera = camera


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1 :])
    recipe = json.loads(args.scenario.read_text())
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    sources, triangles, objects = populate(recipe, args.scenario.parent)
    setup_scene(recipe)
    scene = bpy.context.scene
    shadow_settings = configure_preview(scene, recipe)
    scene.eevee.taa_render_samples = recipe["samples"]
    scene.render.resolution_x, scene.render.resolution_y = recipe["resolution"]
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(args.output.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    began = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    implementation = [
        Path(__file__),
        Path(__file__).with_name("landscape_experiment.py"),
        Path(__file__).with_name("blender_preview.py"),
        Path(__file__).with_name("forest_preview.py"),
        Path(__file__).with_name("forest_views.py"),
        Path(__file__).with_name("organ_preview.py"),
        Path(__file__).with_name("sheet_material.py"),
        Path(__file__).with_name("preview_lighting.py"),
        Path(__file__).with_name("preview_shadows.py"),
    ]
    result = {
        "blender": bpy.app.version_string,
        "engine": scene.render.engine,
        "resolution": recipe["resolution"],
        "samples": recipe["samples"],
        "shadow_terminator": shadow_settings,
        "triangles_instanced": int(triangles),
        "objects": objects,
        "scenario": str(args.scenario),
        "camera": recipe["camera"],
        "render_s": time.perf_counter() - began,
        "input_sha256": {
            str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in [args.scenario, *sources]
        },
        "implementation_sha256": {
            path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in implementation
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": recipe.get("source_modified", False)
        or bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", *map(str, implementation)],
                text=True,
            ).strip()
        ),
        "foliage_lod_quality_proven": False,
    }
    args.output.with_suffix(".json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: result[key]
                for key in (
                    "commit",
                    "source_modified",
                    "render_s",
                    "objects",
                    "triangles_instanced",
                )
            }
        )
    )


if __name__ == "__main__":
    main()
