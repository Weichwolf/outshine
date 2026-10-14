"""Render cached conifer prototypes with bounded point instancing."""

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

import bpy
import numpy as np
from bark_material import apply_bark
from blender_instances import instance_group
from blender_preview import material, mesh_object
from mathutils import Matrix, Vector
from needle_material import apply_needle_materials
from organ_preview import leaf_material, setup_scene
from outshine_ref.contracts import Mesh
from preview_shadows import configure_preview


def populate(recipe, directory):
    paths, triangles, instances = [], 0, 0
    surfaces = {
        "wood": material("young wood", (0.14, 0.105, 0.037), 0.8),
        "leaf": material("needle", (0.024, 0.075, 0.028), 0.59),
        "grass": leaf_material("procedural grass"),
        "ground": material("tile soil", (0.12, 0.105, 0.062), 0.95),
    }
    for product in recipe["products"]:
        mesh_path, poses_path = (
            directory / product["mesh"],
            directory / product["placements"],
        )
        paths.extend((mesh_path, poses_path))
        with np.load(mesh_path) as data:
            mesh = Mesh(data["vertices"], data["faces"])
            original = mesh_object(
                mesh_path.stem,
                mesh.vertices,
                mesh.triangles,
                data["normals"],
                surfaces[product["surface"]],
                corner_normals=True,
            )
            if "corner_uv" in data:
                original.data.uv_layers.new(name="organ coordinates").data.foreach_set(
                    "uv", data["corner_uv"].ravel()
                )
            elif "parameters" in data:
                parameters = data["parameters"][data["faces"].ravel()]
                uv = np.column_stack(((parameters[:, 1] + 1) / 2, parameters[:, 0]))
                original.data.uv_layers.new(name="organ coordinates").data.foreach_set(
                    "uv", uv.ravel()
                )
            triangles += len(mesh.triangles) * product["instances"]
        with np.load(poses_path) as poses:
            count = product["instances"]
            if len(poses["origins"]) != count:
                raise ValueError("declared instance count differs from placements")
            if count == 1:
                transform = np.eye(4)
                transform[:3, :3] = poses["frames"][0] * poses["scales"][0]
                transform[:3, 3] = poses["origins"][0]
                original.matrix_world = Matrix(transform.tolist())
            else:
                instance_group(
                    original,
                    poses["origins"],
                    poses["frames"],
                    poses["scales"],
                    name=mesh_path.stem + " instances",
                )
            instances += count
    paths.extend(apply_bark(recipe, directory))
    paths.extend(apply_needle_materials(recipe, directory))
    return paths, triangles, instances


def setup_scene_lights(recipe):
    previous = set(bpy.context.scene.objects)
    setup_scene(recipe)
    if recipe.get("own_ground", False):
        for obj in tuple(bpy.context.scene.objects):
            if obj not in previous and obj.type == "MESH":
                bpy.data.objects.remove(obj, do_unlink=True)
    fill = next(
        obj
        for obj in bpy.context.scene.objects
        if obj.type == "LIGHT" and obj.data.type == "AREA"
    )
    fill.location = recipe["fill_position_m"]
    fill.rotation_euler = (
        (Vector(recipe["fill_target_m"]) - fill.location)
        .to_track_quat("-Z", "Y")
        .to_euler()
    )
    fill.data.energy, fill.data.size = recipe["fill_energy_w"], recipe["fill_size_m"]


def protect_inputs(output, inputs):
    destinations = {output.resolve(), output.with_suffix(".json").resolve()}
    if any(path.resolve() in destinations for path in inputs):
        raise ValueError("render image and metadata must not overwrite inputs")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1 :])
    protect_inputs(args.output, (args.scenario,))
    recipe = json.loads(args.scenario.read_text())
    if recipe["version"] != "conifer-preview-1":
        raise ValueError("supported complete conifer preview recipe required")
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    began = time.perf_counter()
    paths, triangles, instances = populate(recipe, args.scenario.parent)
    paths.extend(
        args.scenario.parent / recipe[key] for key in ("plant_recipe", "tables")
    )
    protect_inputs(args.output, [args.scenario, *paths])
    setup_scene_lights(recipe)
    scene = bpy.context.scene
    shadows = configure_preview(scene, recipe)
    scene.eevee.taa_render_samples = recipe["samples"]
    scene.render.resolution_x, scene.render.resolution_y = recipe["resolution"]
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(args.output.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    setup = time.perf_counter() - began
    began = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    implementations = [
        Path(__file__).with_name(n)
        for n in (
            "conifer_preview.py",
            "blender_instances.py",
            "blender_preview.py",
            "needle_material.py",
            "bark_material.py",
            "layered_bark_material.py",
            "organ_preview.py",
            "preview_lighting.py",
            "preview_shadows.py",
        )
    ]
    result = {
        "blender": bpy.app.version_string,
        "engine": scene.render.engine,
        "resolution": recipe["resolution"],
        "samples": recipe["samples"],
        "camera": recipe["camera"],
        "shadow_terminator": shadows,
        "instances": instances,
        "shared_meshes": len(recipe["products"]),
        "blender_mesh_objects": sum(o.type == "MESH" for o in scene.objects),
        "triangles_instanced": triangles,
        "setup_s": setup,
        "render_s": time.perf_counter() - began,
        "species": recipe["species"],
        "scope": recipe["scope"],
        "scenario": str(args.scenario),
        "input_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [args.scenario, *paths]
        },
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in implementations
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", *map(str, implementations)],
                text=True,
            ).strip()
        ),
        "plan_commit": recipe["source_commit"],
        "plan_identity": recipe["plan_identity"],
        "plan_source_modified": recipe["source_modified"],
        "foliage_lod_quality_proven": False,
    }
    args.output.with_suffix(".json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: result[k]
                for k in (
                    "instances",
                    "shared_meshes",
                    "blender_mesh_objects",
                    "triangles_instanced",
                    "setup_s",
                    "render_s",
                )
            }
        )
    )


if __name__ == "__main__":
    main()
