"""Render generated branch plans with Blender EEVEE, without image assets."""

import argparse
import json
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


def scene_objects(plans):
    wood = material("wood", (0.16, 0.10, 0.055), 0.85)
    for index, path in enumerate(plans):
        plan = np.load(path)
        vertices, faces, normals = branch_tubes(
            plan["positions"], plan["parents"], plan["radii"]
        )
        vertices[:, 0] += (index - (len(plans) - 1) / 2) * 28
        mesh = bpy.data.meshes.new(path.stem)
        mesh.from_pydata(vertices.tolist(), [], faces.tolist())
        mesh.materials.append(wood)
        mesh.update()
        for polygon in mesh.polygons:
            polygon.use_smooth = True
        mesh.normals_split_custom_set(normals.tolist())
        obj = bpy.data.objects.new(path.stem, mesh)
        bpy.context.collection.objects.link(obj)
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
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=720)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--samples", type=int, default=16)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1 :])
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    scene_objects(args.plans)
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
    result = {
        "blender": bpy.app.version_string,
        "engine": scene.render.engine,
        "resolution": [args.width, args.height],
        "samples": args.samples,
        "plans": [str(p) for p in args.plans],
        "render_s": time.perf_counter() - began,
    }
    args.output.with_suffix(".json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    main()
