"""Read cached own maps without rebuilding noise, curves or organ geometry."""

import json

import bpy
from shoot_form_scene import constant_material


def cached_material(directory):
    recipe = json.loads((directory / "material.json").read_text())
    if recipe["format"] != "procedural-furrow-bark-1":
        raise ValueError("complete procedural furrow material required")
    material = constant_material(
        "own cached procedural bark", (0.12, 0.105, 0.085), 0.82
    )
    nodes, links = material.node_tree.nodes, material.node_tree.links
    shader = nodes["Principled BSDF"]
    textures = {}
    for name in ("colour", "normal_roughness"):
        texture = nodes.new("ShaderNodeTexImage")
        texture.image = bpy.data.images.load(
            str(directory / recipe["maps"][name]["file"])
        )
        texture.image.colorspace_settings.name = "Non-Color"
        texture.extension = "REPEAT"
        textures[name] = texture
    links.new(textures["colour"].outputs["Color"], shader.inputs["Base Color"])
    textures["normal_roughness"].image.alpha_mode = "CHANNEL_PACKED"
    links.new(textures["normal_roughness"].outputs["Alpha"], shader.inputs["Roughness"])
    normal = nodes.new("ShaderNodeNormalMap")
    links.new(textures["normal_roughness"].outputs["Color"], normal.inputs["Color"])
    links.new(normal.outputs["Normal"], shader.inputs["Normal"])
    return material
