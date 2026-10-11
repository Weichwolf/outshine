"""Blender preview of unlit, coverage-weighted procedural organ charts."""

import bpy
from blender_preview import material


def sheet_material(name, directory, maps, card):
    result = material(name, (0.055, 0.16, 0.017), 0.48)
    result.surface_render_method = "DITHERED"
    result.use_backface_culling = False
    nodes, links = result.node_tree.nodes, result.node_tree.links
    textures = {}
    for key in ("colour", "normal") if card else ("colour",):
        filename = maps[key]
        texture = nodes.new("ShaderNodeTexImage")
        texture.image = bpy.data.images.load(
            str((directory / filename).resolve()), check_existing=True
        )
        texture.image.colorspace_settings.name = "Non-Color"
        texture.image.alpha_mode = "CHANNEL_PACKED"
        texture.interpolation = "Linear"
        texture.extension = "EXTEND"
        textures[key] = texture
    alpha = textures["colour"].outputs["Alpha"]
    safe = nodes.new("ShaderNodeMath")
    safe.operation = "MAXIMUM"
    safe.inputs[1].default_value = 1e-5
    links.new(alpha, safe.inputs[0])
    triplet = nodes.new("ShaderNodeCombineXYZ")
    for socket in triplet.inputs:
        links.new(safe.outputs[0], socket)
    colour = nodes.new("ShaderNodeVectorMath")
    colour.operation = "DIVIDE"
    links.new(textures["colour"].outputs["Color"], colour.inputs[0])
    links.new(triplet.outputs[0], colour.inputs[1])
    shader = nodes.get("Principled BSDF")
    links.new(colour.outputs[0], shader.inputs["Base Color"])
    translucent = nodes.new("ShaderNodeBsdfTranslucent")
    links.new(colour.outputs[0], translucent.inputs["Color"])
    if card:
        encoded = nodes.new("ShaderNodeVectorMath")
        encoded.operation = "DIVIDE"
        links.new(textures["normal"].outputs["Color"], encoded.inputs[0])
        links.new(triplet.outputs[0], encoded.inputs[1])
        normal = nodes.new("ShaderNodeNormalMap")
        normal.space = "TANGENT"
        links.new(encoded.outputs[0], normal.inputs["Color"])
        links.new(normal.outputs[0], shader.inputs["Normal"])
        links.new(normal.outputs[0], translucent.inputs["Normal"])
    mixed = nodes.new("ShaderNodeMixShader")
    mixed.inputs[0].default_value = 0.22
    links.new(shader.outputs[0], mixed.inputs[1])
    links.new(translucent.outputs[0], mixed.inputs[2])
    output = mixed.outputs[0]
    if card:
        transparent = nodes.new("ShaderNodeBsdfTransparent")
        coverage = nodes.new("ShaderNodeMixShader")
        links.new(alpha, coverage.inputs[0])
        links.new(transparent.outputs[0], coverage.inputs[1])
        links.new(output, coverage.inputs[2])
        output = coverage.outputs[0]
    links.new(output, nodes.get("Material Output").inputs["Surface"])
    return result
