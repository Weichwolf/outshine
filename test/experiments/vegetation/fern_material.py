"""Local Blender leaf material with explicit source-normal attributes before bump."""

import json

import bpy
import numpy as np
from blender_preview import material
from render_biome_tiles import materials

NORMAL_ATTRIBUTE = "fern_source_normal"


def attach_normals(obj, normals):
    attr = obj.data.attributes.new(NORMAL_ATTRIBUTE, "FLOAT_VECTOR", "CORNER")
    attr.data.foreach_set("vector", np.asarray(normals, np.float32).ravel())


def source_normal(result):
    nodes, links = result.node_tree.nodes, result.node_tree.links
    attribute = nodes.new("ShaderNodeAttribute")
    attribute.attribute_name = NORMAL_ATTRIBUTE
    world = nodes.new("ShaderNodeVectorTransform")
    world.vector_type = "NORMAL"
    world.convert_from, world.convert_to = "OBJECT", "WORLD"
    links.new(attribute.outputs["Vector"], world.inputs[0])
    unit = nodes.new("ShaderNodeVectorMath")
    unit.operation = "NORMALIZE"
    links.new(world.outputs[0], unit.inputs[0])
    return unit.outputs["Vector"]


def leaf_material(inputs):
    metadata = json.loads((inputs / "fern-material.json").read_text())
    result = material("own fern pinnule", (0.04, 0.16, 0.018), 0.55)
    nodes, links = result.node_tree.nodes, result.node_tree.links
    shader = nodes["Principled BSDF"]
    shader.inputs["Specular IOR Level"].default_value = 0.2
    colour = nodes.new("ShaderNodeTexImage")
    colour.image = bpy.data.images.load(str(inputs / metadata["colour"]))
    height = nodes.new("ShaderNodeTexImage")
    height.image = bpy.data.images.load(str(inputs / metadata["height"]))
    for image in (colour.image, height.image):
        image.colorspace_settings.name = "Non-Color"
    links.new(colour.outputs["Color"], shader.inputs["Base Color"])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Distance"].default_value = metadata["height_scale_m"]
    bump.inputs["Strength"].default_value = 0.25
    links.new(height.outputs["Color"], bump.inputs["Height"])
    links.new(source_normal(result), bump.inputs["Normal"])
    links.new(bump.outputs["Normal"], shader.inputs["Normal"])
    thin = nodes.new("ShaderNodeBsdfTranslucent")
    links.new(colour.outputs["Color"], thin.inputs["Color"])
    links.new(bump.outputs["Normal"], thin.inputs["Normal"])
    mix = nodes.new("ShaderNodeMixShader")
    mix.inputs[0].default_value = 0.12
    links.new(shader.outputs[0], mix.inputs[1])
    links.new(thin.outputs[0], mix.inputs[2])
    links.new(mix.outputs[0], nodes["Material Output"].inputs["Surface"])
    return result


def palette(inputs):
    result = materials("Laubwald")
    stem = material("fern rachis", (0.045, 0.13, 0.018), 0.65)
    stem.node_tree.links.new(
        source_normal(stem), stem.node_tree.nodes["Principled BSDF"].inputs["Normal"]
    )
    return [*result, stem, leaf_material(inputs)]
