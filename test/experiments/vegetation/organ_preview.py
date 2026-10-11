"""Render declared organ groups with shared geometry and procedural materials."""

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).parent))
from blender_preview import material, mesh_object
from sheet_material import sheet_material


def leaf_material(name):
    result = material(name, (0.055, 0.16, 0.017), 0.48)
    nodes, links = result.node_tree.nodes, result.node_tree.links
    uv = nodes.new("ShaderNodeTexCoord")
    coordinates = nodes.new("ShaderNodeSeparateXYZ")
    links.new(uv.outputs["UV"], coordinates.inputs[0])
    variation = nodes.new("ShaderNodeObjectInfo")
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (0.022, 0.075, 0.009, 1)
    ramp.color_ramp.elements[1].color = (0.11, 0.23, 0.025, 1)
    links.new(variation.outputs["Random"], ramp.inputs[0])
    centre = nodes.new("ShaderNodeMath")
    centre.operation = "SUBTRACT"
    centre.inputs[1].default_value = 0.5
    links.new(coordinates.outputs["X"], centre.inputs[0])
    distance = nodes.new("ShaderNodeMath")
    distance.operation = "ABSOLUTE"
    links.new(centre.outputs[0], distance.inputs[0])
    vein = nodes.new("ShaderNodeValToRGB")
    vein.color_ramp.elements[0].position = 0.002
    vein.color_ramp.elements[0].color = (1, 1, 1, 1)
    vein.color_ramp.elements[1].position = 0.014
    vein.color_ramp.elements[1].color = (0, 0, 0, 1)
    links.new(distance.outputs[0], vein.inputs[0])
    color = nodes.new("ShaderNodeMixRGB")
    links.new(vein.outputs["Color"], color.inputs[0])
    links.new(ramp.outputs["Color"], color.inputs[1])
    color.inputs[2].default_value = (0.17, 0.26, 0.046, 1)
    shader = nodes.get("Principled BSDF")
    links.new(color.outputs[0], shader.inputs["Base Color"])
    translucent = nodes.new("ShaderNodeBsdfTranslucent")
    links.new(color.outputs[0], translucent.inputs["Color"])
    mixed = nodes.new("ShaderNodeMixShader")
    mixed.inputs[0].default_value = 0.22
    links.new(shader.outputs[0], mixed.inputs[1])
    links.new(translucent.outputs[0], mixed.inputs[2])
    links.new(mixed.outputs[0], nodes.get("Material Output").inputs["Surface"])
    return result


def populate(recipe, directory):
    surfaces = {
        "wood": material("young shoot", (0.14, 0.105, 0.037), 0.8),
        "leaf": leaf_material("leaf: reflected and transmitted light"),
        "grass": leaf_material("grass: reflected and transmitted light"),
    }
    paths, triangles, instances = [], 0, 0
    for product in recipe["products"]:
        mesh_path, poses_path = (
            directory / product["mesh"],
            directory / product["placements"],
        )
        paths.extend((mesh_path, poses_path))
        surface = surfaces[product["surface"]]
        if "textures" in product:
            paths.extend(
                directory / filename for filename in product["textures"].values()
            )
            surface = sheet_material(
                mesh_path.stem, directory, product["textures"], product["card"]
            )
        with np.load(mesh_path) as mesh:
            original = mesh_object(
                mesh_path.stem,
                mesh["vertices"],
                mesh["faces"],
                mesh["normals"],
                surface,
                corner_normals=True,
            )
            if "uv" in mesh:
                uv = mesh["uv"][mesh["faces"].ravel()]
                original.data.uv_layers.new(name="organ coordinates").data.foreach_set(
                    "uv", uv.ravel()
                )
            elif "parameters" in mesh:
                parameters = mesh["parameters"][mesh["faces"].ravel()]
                uv = np.column_stack(((parameters[:, 1] + 1) / 2, parameters[:, 0]))
                original.data.uv_layers.new(name="organ coordinates").data.foreach_set(
                    "uv", uv.ravel()
                )
            triangles += len(mesh["faces"]) * product["instances"]
        with np.load(poses_path) as poses:
            if len(poses["origins"]) != product["instances"]:
                raise ValueError("declared instance count differs from placements")
            for index, (origin, frame) in enumerate(
                zip(poses["origins"], poses["frames"])
            ):
                obj = original if index == 0 else original.copy()
                if index:
                    bpy.context.collection.objects.link(obj)
                transform = np.eye(4)
                transform[:3, :3], transform[:3, 3] = frame, origin
                obj.matrix_world = Matrix(transform.tolist())
                instances += 1
    return paths, triangles, instances


def setup_scene(recipe):
    bpy.ops.mesh.primitive_plane_add(size=recipe["ground_size_m"])
    bpy.context.object.data.materials.append(material("soil", (0.14, 0.11, 0.07), 0.95))
    bpy.ops.object.light_add(type="SUN", location=(0, 0, 4))
    bpy.context.object.rotation_euler = (0.62, -0.4, -0.55)
    bpy.context.object.data.energy = 3
    bpy.context.object.data.angle = 0.06
    bpy.ops.object.light_add(type="AREA", location=(-2, -1, 3))
    fill = bpy.context.object
    fill.rotation_euler = (-fill.location).to_track_quat("-Z", "Y").to_euler()
    fill.data.energy, fill.data.size = 90, 4
    fill.data.color = (0.75, 0.85, 1)
    world = bpy.context.scene.world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (
        0.55,
        0.68,
        0.82,
        1,
    )
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.5
    bpy.ops.object.camera_add(location=recipe["camera"])
    camera = bpy.context.object
    camera.rotation_euler = (
        (Vector(recipe["target"]) - camera.location).to_track_quat("-Z", "Y").to_euler()
    )
    camera.data.sensor_fit, camera.data.sensor_height = "VERTICAL", 24
    camera.data.lens = 12 / np.tan(np.deg2rad(recipe["fov_y"]) / 2)
    camera.data.clip_start, camera.data.clip_end = 0.01, recipe.get("clip_end_m", 50)
    bpy.context.scene.camera = camera


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1 :])
    recipe = json.loads(args.scenario.read_text())
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    paths, triangles, instances = populate(recipe, args.scenario.parent)
    setup_scene(recipe)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
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
        Path(__file__).with_name("blender_preview.py"),
        Path(__file__).with_name("sheet_material.py"),
    ]
    result = {
        "blender": bpy.app.version_string,
        "engine": scene.render.engine,
        "resolution": recipe["resolution"],
        "samples": recipe["samples"],
        "objects": instances,
        "shared_meshes": len(recipe["products"]),
        "triangles_instanced": triangles,
        "scenario": str(args.scenario),
        "camera": recipe["camera"],
        "render_s": time.perf_counter() - began,
        "input_sha256": {
            str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in [args.scenario, *paths]
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
        "plan_commit": recipe["source_commit"],
        "plan_identity": recipe.get("plan_identity"),
        "plan_source_modified": recipe.get("source_modified"),
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
                    "objects",
                    "shared_meshes",
                    "triangles_instanced",
                    "render_s",
                )
            }
        )
    )


if __name__ == "__main__":
    main()
