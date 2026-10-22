"""Continuous cutout masks from categorical ready height fields; no baked light."""

import bpy
import numpy as np


def material(product, chunk=None):
    result = bpy.data.materials.new("same neutral city PBR")
    result.use_nodes = True
    result.use_backface_culling = True
    principled = result.node_tree.nodes.get("Principled BSDF")
    principled.inputs["Base Color"].default_value = (0.6, 0.6, 0.6, 1)
    principled.inputs["Metallic"].default_value = 0
    principled.inputs["Roughness"].default_value = 0.7
    if chunk is None:
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

    image = bpy.data.images.new(
        "unlit discrete city heights",
        width=product.side,
        height=product.side,
        alpha=True,
        float_buffer=True,
    )
    image.colorspace_settings.name = "Non-Color"
    height = product.heights[product.tiers[chunk]].astype(np.float32)
    rgba = np.ones((product.side, product.side, 4), np.float32)
    rgba[:, :, :3] = height[:, :, None]
    image.pixels.foreach_set(rgba.ravel())
    image.update()
    texture = nodes.new("ShaderNodeTexImage")
    texture.image, texture.interpolation, texture.extension = image, "Closest", "CLIP"
    uv = nodes.new("ShaderNodeUVMap")
    uv.uv_map = "grid uv"
    links.new(uv.outputs["UV"], texture.inputs["Vector"])
    colour = nodes.new("ShaderNodeSeparateColor")
    links.new(texture.outputs["Color"], colour.inputs[0])
    height_value = colour.outputs[0]
    split = nodes.new("ShaderNodeSeparateXYZ")
    links.new(uv.outputs["UV"], split.inputs[0])
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
        for chunk in chunks:
            mesh.materials.append(material(product, chunk))
        mesh.polygons.foreach_set("material_index", arrays["material"])
    else:
        mesh.materials.append(material(product))
    obj = bpy.data.objects.new("ready city", mesh)
    scene.collection.objects.link(obj)
    return {
        "quads": len(arrays["quads"]),
        "materials": len(mesh.materials),
        "masked": masked,
        "diagnostic_image_payload_bytes": len(chunks) * product.side**2 * 4 * 4
        if masked
        else 0,
        "mask_rule": "Categorical nearest height; continuous footprint/height tests. Dithered camera and shadow alpha; no baked lighting.",
    }
