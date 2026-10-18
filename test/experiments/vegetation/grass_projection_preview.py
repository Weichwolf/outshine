"""Matched unlit physical/cached gap diagnostics; deliberately no canopy BSDF."""

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

GRASS = (0.08, 0.22, 0.025, 1)
GROUND = (0.22, 0.13, 0.055, 1)


def emission(name, colour):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    nodes.clear()
    shader = nodes.new("ShaderNodeEmission")
    shader.inputs["Color"].default_value = colour
    output = nodes.new("ShaderNodeOutputMaterial")
    mat.node_tree.links.new(shader.outputs[0], output.inputs["Surface"])
    return mat, shader, output


def object_mesh(name, vertices, faces, uv, mat):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices.tolist(), [], faces.tolist())
    mesh.materials.append(mat)
    mesh.uv_layers.new(name="ground footprint").data.foreach_set(
        "uv", uv[faces].ravel()
    )
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def blade_material():
    mat, shader, output = emission("unlit physical opaque blades", GRASS)
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    uv = nodes.new("ShaderNodeSeparateXYZ")
    links.new(nodes.new("ShaderNodeTexCoord").outputs["UV"], uv.inputs[0])
    position = nodes.new("ShaderNodeSeparateXYZ")
    links.new(
        nodes.new("ShaderNodeNewGeometry").outputs["Position"], position.inputs[0]
    )
    mask = None
    for coordinate, operation, threshold in [
        (uv.outputs[0], "GREATER_THAN", 0),
        (uv.outputs[0], "LESS_THAN", 1),
        (uv.outputs[1], "GREATER_THAN", 0),
        (uv.outputs[1], "LESS_THAN", 1),
        (position.outputs[2], "GREATER_THAN", 0),
    ]:
        test = nodes.new("ShaderNodeMath")
        test.operation = operation
        links.new(coordinate, test.inputs[0])
        test.inputs[1].default_value = threshold
        if mask is None:
            mask = test.outputs[0]
        else:
            product = nodes.new("ShaderNodeMath")
            product.operation = "MULTIPLY"
            links.new(mask, product.inputs[0])
            links.new(test.outputs[0], product.inputs[1])
            mask = product.outputs[0]
    mix = nodes.new("ShaderNodeMixShader")
    links.new(mask, mix.inputs[0])
    links.new(nodes.new("ShaderNodeBsdfTransparent").outputs[0], mix.inputs[1])
    links.new(shader.outputs[0], mix.inputs[2])
    links.new(mix.outputs[0], output.inputs["Surface"])
    mat.surface_render_method = "DITHERED"
    return mat


def cached_material(coverage):
    mat, shader, _ = emission("unlit cached footprint contacts", GROUND)
    image = bpy.data.images.new(
        "generated contact mask",
        width=len(coverage),
        height=len(coverage),
        float_buffer=True,
    )
    image.colorspace_settings.name = "Non-Color"
    rgb = np.asarray(GROUND) + coverage[..., None] * (np.asarray(GRASS) - GROUND)
    image.pixels.foreach_set(rgb.astype(np.float32).ravel())
    texture = mat.node_tree.nodes.new("ShaderNodeTexImage")
    texture.image, texture.interpolation = image, "Closest"
    mat.node_tree.links.new(texture.outputs["Color"], shader.inputs["Color"])
    return mat


def render_view(report, stand, view, name, output):
    began = time.perf_counter()
    direction = np.asarray(
        next(r["direction"] for r in report["views"] if r["name"] == name)
    )
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.object.camera_add(location=tuple(4 * direction))
    camera = bpy.context.object
    camera.rotation_euler = (-Vector(direction)).to_track_quat("-Z", "Y").to_euler()
    right = np.asarray(camera.rotation_euler.to_matrix() @ Vector((1, 0, 0)))
    camera.data.type, camera.data.ortho_scale = "ORTHO", 2.8
    scene = bpy.context.scene
    scene.camera = camera
    lower, upper = np.asarray(report["footprint_m"])
    vertices, faces = stand["vertices"], stand["faces"]
    uv = (
        vertices[:, :2] - vertices[:, 2, None] * direction[:2] / direction[2] - lower
    ) / (upper - lower)
    possible = np.all(uv[faces].max(1) >= 0, axis=1) & np.all(
        uv[faces].min(1) <= 1, axis=1
    )
    object_mesh(
        "physical cached blades left",
        vertices - 0.65 * right,
        faces[possible],
        uv,
        blade_material(),
    )
    quad = np.array(
        ((*lower, 0), (upper[0], lower[1], 0), (*upper, 0), (lower[0], upper[1], 0))
    )
    qfaces, quv = (
        np.array(((0, 1, 2), (0, 2, 3))),
        np.array(((0, 0), (1, 0), (1, 1), (0, 1))),
    )
    ground, _, _ = emission("unlit substrate", GROUND)
    object_mesh("physical substrate left", quad - 0.65 * right, qfaces, quv, ground)
    object_mesh(
        "cached footprint right",
        quad + 0.65 * right,
        qfaces,
        quv,
        cached_material(view["coverage"]),
    )
    scene.world = bpy.data.worlds.new("diagnostic background")
    scene.world.color = (0.04, 0.055, 0.075)
    configure_preview(scene)
    scene.eevee.taa_render_samples = 4
    scene.render.resolution_x, scene.render.resolution_y = 1920, 1080
    scene.render.resolution_percentage = 100
    scene.view_settings.view_transform = "Standard"
    scene.render.image_settings.file_format = "PNG"
    target = output / f"{name}.png"
    scene.render.filepath = str(target)
    bpy.ops.render.render(write_still=True)
    return {
        "name": name,
        "left": "physical geometry",
        "right": "cached opaque contact mask",
        "direction": direction.tolist(),
        "profile": [1920, 1080],
        "samples": 4,
        "wall_s": time.perf_counter() - began,
        "image_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
        "renderer": gpu.platform.renderer_get(),
        "backend": gpu.platform.backend_type_get(),
        "scope": "Unlit fixed-direction footprint equality diagnostic. Not natural grass lighting, parallax, full silhouette or native LOD proof",
    }


def render(inputs, output):
    if output.exists():
        raise ValueError("fresh diagnostic output directory required")
    report, stand, views = read_product(inputs)
    output.mkdir(parents=True)
    rows = [
        render_view(report, stand, views[name], name, output)
        for name in ("top", "east")
    ]
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "stand_identity": report["stand_identity"],
        "views": rows,
    }
    (output / "render.json").write_text(json.dumps(result, indent=2))
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    render(args.inputs, args.output)
