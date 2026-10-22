"""Continuous cutout masks from categorical ready height fields; no baked light."""

import bpy
import numpy as np
from urban_patch_mesh import height_atlas


def material(product, chunks=None):
    result = bpy.data.materials.new("same neutral city PBR")
    result.use_nodes = True
    result.use_backface_culling = True
    principled = result.node_tree.nodes.get("Principled BSDF")
    principled.inputs["Base Color"].default_value = (0.6, 0.6, 0.6, 1)
    principled.inputs["Metallic"].default_value = 0
    principled.inputs["Roughness"].default_value = 0.7
    if chunks is None:
        return result
    result.surface_render_method = "DITHERED"
    result.use_transparent_shadow = True
    nodes, links = result.node_tree.nodes, result.node_tree.links

    def math(operation, *values):
        node = nodes.new("ShaderNodeMath")
        node.operation = operation
        for socket, value in zip(node.inputs, values):
            if isinstance(value, (int, float)):
                socket.default_value = value
            else:
                links.new(value, socket)
        return node.outputs[0]

    def attribute(name):
        node = nodes.new("ShaderNodeAttribute")
        node.attribute_name = name
        return node.outputs["Fac"]

    rgba, columns, rows = height_atlas(product, chunks)
    image = bpy.data.images.new(
        "unlit discrete city heights",
        width=rgba.shape[1],
        height=rgba.shape[0],
        alpha=True,
        float_buffer=True,
    )
    image.colorspace_settings.name = "Non-Color"
    image.pixels.foreach_set(rgba.ravel())
    image.update()
    result["height_field_payload_bytes"] = int(rgba.nbytes)
    texture = nodes.new("ShaderNodeTexImage")
    texture.image, texture.interpolation, texture.extension = image, "Closest", "CLIP"
    uv = nodes.new("ShaderNodeUVMap")
    uv.uv_map = "grid uv"
    colour = nodes.new("ShaderNodeSeparateColor")
    links.new(texture.outputs["Color"], colour.inputs[0])
    height_value = colour.outputs[0]
    split = nodes.new("ShaderNodeSeparateXYZ")
    links.new(uv.outputs["UV"], split.inputs[0])
    atlas = nodes.new("ShaderNodeCombineXYZ")
    patch = math("ROUND", attribute("patch index"))
    column = math("MODULO", patch, columns)
    row = math("FLOOR", math("DIVIDE", patch, columns))
    links.new(
        math("DIVIDE", math("ADD", split.outputs[0], column), columns), atlas.inputs[0]
    )
    links.new(math("DIVIDE", math("ADD", split.outputs[1], row), rows), atlas.inputs[1])
    links.new(atlas.outputs[0], texture.inputs["Vector"])
    kind = attribute("plane kind")
    roof_height = attribute("plane height")
    margins = (1 - float(product.frame[4])) / 2
    mask = math("GREATER_THAN", height_value, 0)
    for axis in range(2):
        phase = math("FRACT", math("MULTIPLY", split.outputs[axis], product.side))
        inside = math(
            "MULTIPLY",
            math("GREATER_THAN", phase, margins),
            math("LESS_THAN", phase, 1 - margins),
        )
        wall_axis = math("COMPARE", kind, axis + 1, 0.01)
        mask = math("MULTIPLY", mask, math("MAXIMUM", wall_axis, inside))
    position = nodes.new("ShaderNodeNewGeometry")
    xyz = nodes.new("ShaderNodeSeparateXYZ")
    links.new(position.outputs["Position"], xyz.inputs[0])
    vertical = math("LESS_THAN", xyz.outputs[2], height_value)
    roof = math("COMPARE", height_value, roof_height, 0.01)
    floor = math("COMPARE", kind, 3, 0.01)
    is_roof = math("COMPARE", kind, 0, 0.01)
    conditional = math(
        "ADD",
        math("MULTIPLY", is_roof, roof),
        math(
            "MULTIPLY", math("SUBTRACT", 1, is_roof), math("MAXIMUM", floor, vertical)
        ),
    )
    links.new(math("MULTIPLY", mask, conditional), principled.inputs["Alpha"])
    return result


def install(scene, arrays, product, chunks, masked):
    mesh = bpy.data.meshes.new("same ready city geometry")
    mesh.from_pydata(arrays["positions"].tolist(), [], arrays["quads"].tolist())
    mesh.update()
    uv = mesh.uv_layers.new(name="grid uv")
    uv.data.foreach_set("uv", arrays["grid_uv"].ravel())
    for key, label in [("plane_kind", "plane kind"), ("plane_height", "plane height")]:
        attribute = mesh.attributes.new(label, "FLOAT", "POINT")
        attribute.data.foreach_set("value", arrays[key])
    if masked:
        attribute = mesh.attributes.new("patch index", "FLOAT", "POINT")
        attribute.data.foreach_set(
            "value", np.repeat(arrays["material"], 4).astype(np.float32)
        )
        mesh.materials.append(material(product, chunks))
    else:
        mesh.materials.append(material(product))
    obj = bpy.data.objects.new("ready city", mesh)
    scene.collection.objects.link(obj)
    return {
        "quads": len(arrays["quads"]),
        "materials": len(mesh.materials),
        "masked": masked,
        "diagnostic_image_payload_bytes": int(
            mesh.materials[0].get("height_field_payload_bytes", 0)
        ),
        "mask_rule": "Categorical nearest height; continuous footprint/height tests. Dithered camera and shadow alpha; no baked lighting.",
    }
