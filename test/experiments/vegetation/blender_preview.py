"""Render generated branch plans with Blender EEVEE, without image assets."""

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
from wood_mesh import branch_tubes


def material(name, color, roughness):
    result = bpy.data.materials.new(name)
    result.use_nodes = True
    shader = result.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*color, 1)
    shader.inputs["Roughness"].default_value = roughness
    return result


def foliage_material():
    result = material("foliage envelope", (0.04, 0.12, 0.018), 0.9)
    nodes, links = result.node_tree.nodes, result.node_tree.links
    geometry = nodes.new("ShaderNodeNewGeometry")
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 1.8
    noise.inputs["Detail"].default_value = 3
    noise.inputs["Roughness"].default_value = 0.7
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (0.018, 0.052, 0.008, 1)
    ramp.color_ramp.elements[1].color = (0.12, 0.24, 0.035, 1)
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.2
    bump.inputs["Distance"].default_value = 0.15
    shader = nodes.get("Principled BSDF")
    links.new(geometry.outputs["Position"], noise.inputs["Vector"])
    links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    links.new(ramp.outputs["Color"], shader.inputs["Base Color"])
    links.new(noise.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], shader.inputs["Normal"])
    return result


def mesh_object(name, vertices, faces, normals, surface, corner_normals=False):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices.tolist(), [], faces.tolist())
    mesh.materials.append(surface)
    mesh.update()
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    if corner_normals:
        mesh.normals_split_custom_set(normals.tolist())
    else:
        mesh.normals_split_custom_set_from_vertices(normals.tolist())
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def scene_objects(plans, crowns):
    wood = material("wood", (0.16, 0.10, 0.055), 0.85)
    for index, path in enumerate(plans):
        plan = np.load(path)
        vertices, faces, normals = branch_tubes(
            plan["positions"], plan["parents"], plan["radii"]
        )
        vertices[:, 0] += (index - (len(plans) - 1) / 2) * 28
        mesh_object(path.stem, vertices, faces, normals, wood, corner_normals=True)
    foliage = foliage_material() if crowns else None
    for index, path in enumerate(crowns):
        with np.load(path) as crown:
            vertices = crown["vertices"].copy()
            vertices[:, 0] += (index - (len(plans) - 1) / 2) * 28
            mesh_object(path.stem, vertices, crown["faces"], crown["normals"], foliage)
    bpy.ops.mesh.primitive_plane_add(size=250)
    bpy.context.object.data.materials.append(
        material("ground", (0.36, 0.39, 0.35), 0.95)
    )
    bpy.ops.object.light_add(type="SUN", location=(0, 0, 40))
    bpy.context.object.rotation_euler = (0.42, -0.38, -0.40)
    bpy.context.object.data.energy = 3.0
    bpy.context.object.data.angle = 0.08
    bpy.context.scene.world.use_nodes = True
    bpy.context.scene.world.node_tree.nodes["Background"].inputs[
        "Color"
    ].default_value = (0.65, 0.76, 0.95, 1)
    bpy.context.scene.world.node_tree.nodes["Background"].inputs[
        "Strength"
    ].default_value = 0.6
    bpy.ops.object.camera_add(location=(12, -58, 27))
    camera = bpy.context.object
    camera.rotation_euler = (
        (Vector((0, 0, 12)) - camera.location).to_track_quat("-Z", "Y").to_euler()
    )
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 31 if len(plans) == 1 else len(plans) * 29
    bpy.context.scene.camera = camera


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plans", type=Path, nargs="+", required=True)
    parser.add_argument("--crowns", type=Path, nargs="+", default=[])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=720)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--samples", type=int, default=16)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1 :])
    if args.crowns and len(args.crowns) != len(args.plans):
        parser.error("one optional crown per branch plan required")
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    scene_objects(args.plans, args.crowns)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.eevee.taa_render_samples = args.samples
    scene.render.resolution_x, scene.render.resolution_y = args.width, args.height
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(args.output.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    began = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    render_seconds = time.perf_counter() - began
    result = {
        "blender": bpy.app.version_string,
        "engine": scene.render.engine,
        "resolution": [args.width, args.height],
        "samples": args.samples,
        "plans": [str(p) for p in args.plans],
        "crowns": [str(p) for p in args.crowns],
        "input_sha256": {
            str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (*args.plans, *args.crowns)
        },
        "implementation_sha256": {
            path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in Path(__file__).parent.glob("*.py")
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True).strip()
        ),
        "camera": {
            "projection": scene.camera.data.type,
            "ortho_scale_m": scene.camera.data.ortho_scale,
            "position_m": list(scene.camera.location),
        },
        "geometric_pixel_error_bound_verified_for_render": False,
        "render_s": render_seconds,
    }
    args.output.with_suffix(".json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    main()
