"""Physical Lambert/thin-sheet Blender reference and direct-light diagnostic maps."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from grass_projection_reader import read_product
from mathutils import Vector
from preview_shadows import configure_preview


def mesh_object(name, vertices, faces, mat, uv=None):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices.tolist(), [], faces.tolist())
    mesh.materials.append(mat)
    if uv is not None:
        mesh.uv_layers.new(name="ground coordinates").data.foreach_set(
            "uv", uv[faces].ravel()
        )
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def sheet(name, reflection, transmission=None):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    diffuse = nodes.new("ShaderNodeBsdfDiffuse")
    diffuse.inputs["Color"].default_value = (*reflection, 1)
    diffuse.inputs["Roughness"].default_value = 0
    geometry = nodes.new("ShaderNodeNewGeometry")
    links.new(geometry.outputs["True Normal"], diffuse.inputs["Normal"])
    if transmission is None:
        links.new(diffuse.outputs[0], output.inputs["Surface"])
    else:
        translucent = nodes.new("ShaderNodeBsdfTranslucent")
        translucent.inputs["Color"].default_value = (*transmission, 1)
        links.new(geometry.outputs["True Normal"], translucent.inputs["Normal"])
        add = nodes.new("ShaderNodeAddShader")
        links.new(diffuse.outputs[0], add.inputs[0])
        links.new(translucent.outputs[0], add.inputs[1])
        links.new(add.outputs[0], output.inputs["Surface"])
    return mat


def frame_material(rgb):
    mat = bpy.data.materials.new(
        "temporary direct-light diagnostic, not cached illumination"
    )
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    image = bpy.data.images.new(
        "relative linear radiance", width=len(rgb), height=len(rgb), float_buffer=True
    )
    image.colorspace_settings.name = "Non-Color"
    image.pixels.foreach_set(
        np.dstack((rgb, np.ones(rgb.shape[:2]))).astype(np.float32).ravel()
    )
    texture = nodes.new("ShaderNodeTexImage")
    texture.image, texture.interpolation = image, "Closest"
    emission = nodes.new("ShaderNodeEmission")
    links.new(texture.outputs["Color"], emission.inputs["Color"])
    output = nodes.new("ShaderNodeOutputMaterial")
    links.new(emission.outputs[0], output.inputs["Surface"])
    return mat


def setup(view):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.world = bpy.data.worlds.new("declared directional quadrature only")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0
    bpy.ops.object.camera_add(location=tuple(4 * view))
    camera = bpy.context.object
    camera.rotation_euler = (-Vector(view)).to_track_quat("-Z", "Y").to_euler()
    camera.data.type, camera.data.ortho_scale = "ORTHO", 1.4
    scene.camera = camera
    scene.render.resolution_x, scene.render.resolution_y = 1920, 1080
    scene.render.resolution_percentage = 100
    scene.view_settings.view_transform = "Standard"
    scene.render.image_settings.file_format = "PNG"
    return scene


def render_case(stand, result, state_name, mode, inputs, output):
    began = time.perf_counter()
    view = np.asarray(result["view"])
    scene = setup(view)
    state = result["states"][state_name]
    if mode == "physical":
        mesh_object(
            "same saved blades",
            stand["vertices"],
            stand["faces"],
            sheet("diffuse thin blades", state["leaf_rt"][:3], state["leaf_rt"][3:]),
        )
        mesh_object(
            "flat substrate",
            np.array(((-3, -3, 0), (3, -3, 0), (3, 3, 0), (-3, 3, 0))),
            np.array(((0, 1, 2), (0, 2, 3))),
            sheet("diffuse ground", state["ground_albedo"]),
        )
        for lamp in state["lights"]:
            bpy.ops.object.light_add(type="SUN")
            obj = bpy.context.object
            obj.rotation_euler = (
                (-Vector(lamp["direction"])).to_track_quat("-Z", "Y").to_euler()
            )
            energy = max(lamp["irradiance_rgb"])
            obj.data.energy = energy
            obj.data.color = tuple(np.asarray(lamp["irradiance_rgb"]) / energy)
            obj.data.angle = 0
    else:
        with np.load(inputs / f"{state_name}.npz") as data:
            radiance = data[mode]
        lower, upper = np.asarray(result["footprint_m"])
        vertices = np.array(
            ((*lower, 0), (upper[0], lower[1], 0), (*upper, 0), (lower[0], upper[1], 0))
        )
        mesh_object(
            "per-frame direct light map",
            vertices,
            np.array(((0, 1, 2), (0, 2, 3))),
            frame_material(radiance),
            np.array(((0, 0), (1, 0), (1, 1), (0, 1))),
        )
    configure_preview(scene)
    scene.eevee.taa_render_samples = 8
    target = output / f"{state_name}-{mode}.png"
    scene.render.filepath = str(target)
    bpy.ops.render.render(write_still=True)
    return {
        "state": state_name,
        "mode": mode,
        "wall_s": time.perf_counter() - began,
        "profile": [1920, 1080],
        "samples": 8,
        "image_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
        "renderer": gpu.platform.renderer_get(),
        "backend": gpu.platform.backend_type_get(),
    }


def render(inputs, output):
    if output.exists():
        raise ValueError("fresh diagnostic images directory required")
    result = json.loads((inputs / "result.json").read_text())
    source = Path(result["source_inputs"])
    if (
        hashlib.sha256((source / "result.json").read_bytes()).hexdigest()
        != result["source_report_sha256"]
    ):
        raise ValueError("original geometry/contact input changed")
    _, stand, _ = read_product(source)
    output.mkdir(parents=True)
    rows = []
    for state in ("front", "back"):
        for mode in ("physical", "radiance", "independent_mean", "reflection_only"):
            # Map data are frame diagnostics; geometry products never store this light.
            rows.append(render_case(stand, result, state, mode, inputs, output))
            print(json.dumps(rows[-1]), flush=True)
    receipt = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "light_input_sha256": hashlib.sha256(
            (inputs / "result.json").read_bytes()
        ).hexdigest(),
        "stand_identity": result["stand_identity"],
        "views": rows,
        "scope": "Actual opaque-occlusion/diffuse-sheet Blender reference versus per-frame CPU maps. Different shadow discretization, not a numerical transport oracle or full natural grass model.",
    }
    (output / "render.json").write_text(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    render(args.inputs, args.output)
