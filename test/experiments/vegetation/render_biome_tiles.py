"""Four flat tiles show physical Python vegetation; high-resolution masters stay local."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from blender_instances import instance_group
from blender_preview import material, mesh_object
from mathutils import Vector
from preview_shadows import configure_preview
from scipy.spatial.transform import Rotation
from scipy.stats import qmc

HERE = Path(__file__).resolve().parent


def materials(tile):
    dry = tile == "Steppe"
    colors = (
        (0.10, 0.055, 0.023),
        (0.17, 0.14, 0.035) if dry else (0.025, 0.11, 0.014),
        (0.26, 0.21, 0.065) if dry else (0.045, 0.17, 0.023),
        (0.11, 0.13, 0.025) if dry else (0.075, 0.21, 0.032),
        (0.38, 0.035, 0.12),
    )
    result = []
    for index, color in enumerate(colors):
        mat = material(f"procedural organ {index}", color, 0.8 if index == 0 else 0.65)
        nodes, links = mat.node_tree.nodes, mat.node_tree.links
        shader = nodes["Principled BSDF"]
        shader.inputs["Specular IOR Level"].default_value = 0.2
        if index in (1, 2, 3):
            thin = nodes.new("ShaderNodeBsdfTranslucent")
            thin.inputs["Color"].default_value = (*color, 1)
            mix = nodes.new("ShaderNodeMixShader")
            mix.inputs[0].default_value = 0.12
            links.new(shader.outputs[0], mix.inputs[1])
            links.new(thin.outputs[0], mix.inputs[2])
            links.new(mix.outputs[0], nodes["Material Output"].inputs["Surface"])
        texture = nodes.new("ShaderNodeTexNoise")
        texture.inputs["Scale"].default_value = 35 if index else 9
        bump = nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.12
        bump.inputs["Distance"].default_value = 0.0003 if index else 0.008
        links.new(texture.outputs["Fac"], bump.inputs["Height"])
        links.new(bump.outputs[0], shader.inputs["Normal"])
        result.append(mat)
    return result


def poses(xy, rng, scale=1):
    frames = Rotation.from_euler("z", rng.uniform(-np.pi, np.pi, len(xy))).as_matrix()
    return (
        np.c_[xy, np.zeros(len(xy))],
        frames,
        rng.uniform(0.85, 1.15, len(xy)) * scale,
    )


def population(name, recipe):
    rng = np.random.default_rng(recipe["seed"])
    size, step = recipe["size_m"], recipe["grass_step_m"]
    x, y = np.meshgrid(
        np.arange(-size / 2 + step / 2, size / 2, step),
        np.arange(-size / 2 + step / 2, size / 2, step),
    )
    xy = np.c_[x.ravel(), y.ravel()] + rng.uniform(-0.4 * step, 0.4 * step, (x.size, 2))
    result = []
    variants = rng.integers(0, 8, len(xy))
    for index in range(8):
        p, f, s = poses(xy[variants == index], rng)
        scales = np.repeat(s[:, None], 3, axis=1)
        if name == "Steppe":
            scales *= (0.8, 0.8, 1.65)
        if name == "Heide":
            scales *= (0.7, 0.7, 0.6)
        result.append((f"grass-{index}", p, f, scales))
    if recipe["forest"] in ("broadleaf", "mixed"):
        points = qmc.PoissonDisk(2, radius=0.12, rng=recipe["seed"]).fill_space()
        xy = (points - 0.5) * (size - 3)
        xy = xy[xy[:, 1] > -3 + 2 * np.sin(xy[:, 0] * 0.25)]
        kinds = rng.choice(
            ("oak", "beech", "pine")
            if recipe["forest"] == "mixed"
            else ("oak", "beech"),
            len(xy),
        )
        for key in np.unique(kinds):
            result.append(
                (key, *poses(xy[kinds == key], rng, 2.1 if key == "pine" else 1))
            )
    if recipe["forest"] == "heath":
        x, y = np.meshgrid(np.arange(-6.5, 7, 0.9), np.arange(-6.5, 7, 0.9))
        xy = np.c_[x.ravel(), y.ravel()] + rng.uniform(-0.3, 0.3, (x.size, 2))
        selected = np.sin(xy[:, 0] * 0.6) + np.cos(xy[:, 1] * 0.8) > -0.5
        result.append(("heather", *poses(xy[selected], rng, 1.5)))
    return result


def plants(name, recipe, products):
    mats, report = materials(name), []
    for key, origins, frames, scales in population(name, recipe):
        path = products / (key + ".npz")
        with np.load(path) as stored:
            mesh = {key: stored[key] for key in stored.files}
        obj = mesh_object(
            key,
            mesh["vertices"],
            mesh["faces"],
            mesh["normals"],
            mats[0],
            corner_normals=True,
        )
        for mat in mats[1:]:
            obj.data.materials.append(mat)
        obj.data.polygons.foreach_set(
            "material_index", mesh["surface"].astype(np.int32)
        )
        obj.data.uv_layers.new(name="procedural organ UV").data.foreach_set(
            "uv", mesh["uv"].ravel()
        )
        instance_group(obj, origins, frames, scales, name=key + " instances")
        report.append(
            {
                "model": key,
                "instances": len(origins),
                "prototype_triangles": len(mesh["faces"]),
                "product_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            }
        )
    return report


def setup(name, recipe):
    scene = bpy.context.scene
    configure_preview(scene)
    scene.render.resolution_x, scene.render.resolution_y = 1920, 1080
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.eevee.taa_render_samples = 4
    scene.view_settings.view_transform = "AgX"
    size = recipe["size_m"]
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, -0.25))
    bpy.context.object.scale = (size, size, 0.5)
    color = (0.15, 0.11, 0.055) if name == "Steppe" else (0.045, 0.065, 0.022)
    ground = material("procedural flat soil", color, 0.96)
    nodes, links = ground.node_tree.nodes, ground.node_tree.links
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 80
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Distance"].default_value = 0.035
    bump.inputs["Strength"].default_value = 0.3
    links.new(noise.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs[0], nodes["Principled BSDF"].inputs["Normal"])
    bpy.context.object.data.materials.append(ground)
    bpy.ops.mesh.primitive_plane_add(size=300, location=(0, 0, -0.52))
    bpy.context.object.data.materials.append(
        material("studio", (0.085, 0.095, 0.11), 0.9)
    )
    bpy.ops.object.light_add(type="SUN")
    bpy.context.object.rotation_euler = (0.45, -0.65, -0.6)
    bpy.context.object.data.energy, bpy.context.object.data.angle = 2.5, 0.04
    bpy.ops.object.light_add(type="AREA", location=(-8, -12, 18))
    bpy.context.object.data.energy, bpy.context.object.data.shape = 900, "DISK"
    bpy.context.object.data.size = 18
    bpy.context.object.rotation_euler = (
        Vector((8, 12, -18)).to_track_quat("-Z", "Y").to_euler()
    )
    scene.world = bpy.data.worlds.new("neutral research studio")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (
        0.35,
        0.45,
        0.6,
        1,
    )
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.35
    bpy.ops.object.camera_add(location=recipe["camera"])
    cam = bpy.context.object
    cam.rotation_euler = (
        (Vector(recipe["target"]) - cam.location).to_track_quat("-Z", "Y").to_euler()
    )
    cam.data.type, cam.data.ortho_scale = "ORTHO", size * 1.5
    scene.camera = cam
    return scene


def render(name, products, output):
    began = time.perf_counter()
    recipe_path = HERE / "biome_tiles.json"
    recipe = json.loads(recipe_path.read_text())[name]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    models = plants(name, recipe, products)
    scene = setup(name, recipe)
    output.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str((output / (name + ".png")).resolve())
    prepared = time.perf_counter()
    print(
        json.dumps(
            {
                "stage": "render",
                "tile": name,
                "models": models,
                "setup_s": prepared - began,
            }
        ),
        flush=True,
    )
    bpy.ops.render.render(write_still=True)
    report = {
        "tile": name,
        "recipe": recipe,
        "models": models,
        "resolution": [1920, 1080],
        "samples": 4,
        "renderer": gpu.platform.renderer_get(),
        "setup_s": prepared - began,
        "render_s": time.perf_counter() - prepared,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "scope": "Physical current Python forms on flat demonstration tiles. No GIS, native-runtime, botanical, LOD or device acceptance. Heather is a provisional branching/flower form, not an accepted species recipe.",
    }
    (output / (name + ".json")).write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {"tile": name, "setup_s": report["setup_s"], "render_s": report["render_s"]}
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--tile", required=True, choices=("Laubwald", "Mischwald", "Steppe", "Heide")
    )
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    render(args.tile, args.products, args.output)
