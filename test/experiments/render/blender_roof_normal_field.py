"""Display a ready canonical normal atlas on generated roofs; no source or GEOS."""

import hashlib
from pathlib import Path

import bpy
import numpy as np


def install(mesh, material, config):
    path = Path(config["path"])
    with np.load(path, allow_pickle=False) as data:
        pixels = data["pixels"]
    with np.load(config["uv"], allow_pickle=False) as data:
        uv = data["uv"]
    if uv.shape != (len(mesh.polygons), 3, 2):
        raise ValueError("One roof-field UV per actual triangle corner required")
    layer = mesh.uv_layers.new(name="roof field coordinates")
    layer.data.foreach_set("uv", uv.astype(np.float32).ravel())
    height, width = pixels.shape[:2]
    image = bpy.data.images.new(
        "source roof normal field",
        width=width,
        height=height,
        alpha=True,
        float_buffer=False,
    )
    image.colorspace_settings.name = "Non-Color"
    image.pixels.foreach_set((pixels.astype(np.float32) / 255).ravel())
    image.update()
    nodes, links = material.node_tree.nodes, material.node_tree.links
    tex = nodes.new("ShaderNodeTexImage")
    tex.image, tex.extension, tex.interpolation = image, "CLIP", "Closest"
    mapping = nodes.new("ShaderNodeUVMap")
    mapping.uv_map = layer.name
    links.new(mapping.outputs["UV"], tex.inputs["Vector"])
    decode = nodes.new("ShaderNodeVectorMath")
    decode.operation = "MULTIPLY_ADD"
    decode.inputs[1].default_value = (2, 2, 2)
    decode.inputs[2].default_value = (-1, -1, -1)
    links.new(tex.outputs["Color"], decode.inputs[0])
    combine = nodes.new("ShaderNodeCombineXYZ")
    for axis, row in enumerate(config["normal_basis"]):
        dot = nodes.new("ShaderNodeVectorMath")
        dot.operation = "DOT_PRODUCT"
        dot.inputs[1].default_value = row
        links.new(decode.outputs["Vector"], dot.inputs[0])
        links.new(dot.outputs["Value"], combine.inputs[axis])
    geometry = nodes.new("ShaderNodeNewGeometry")
    mix = nodes.new("ShaderNodeMixRGB")
    mix.blend_type, mix.use_clamp = "MIX", False
    links.new(tex.outputs["Alpha"], mix.inputs[0])
    links.new(geometry.outputs["Normal"], mix.inputs[1])
    links.new(combine.outputs["Vector"], mix.inputs[2])
    normalize = nodes.new("ShaderNodeVectorMath")
    normalize.operation = "NORMALIZE"
    links.new(mix.outputs["Color"], normalize.inputs[0])
    links.new(
        normalize.outputs["Vector"], nodes.get("Principled BSDF").inputs["Normal"]
    )
    return {
        "atlas_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "uv_sha256": hashlib.sha256(Path(config["uv"]).read_bytes()).hexdigest(),
        "size": [width, height],
        "input_rgba8_bytes": pixels.nbytes,
        "uv_bytes": uv.nbytes,
        "image_is_float": image.is_float,
        "image_depth_bits": image.depth,
        "interpolation": tex.interpolation,
        "texture_extension": tex.extension,
        "scope": "Only shading normal override on eligible roof UVs. Blender image and staging/driver memory differ from native residency; filtering/NDF, alpha, depth and procedural materials unqualified.",
    }
