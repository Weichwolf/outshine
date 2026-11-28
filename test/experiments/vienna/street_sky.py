"""Replace the probe-based diffuse sky term at traced street receivers."""

from time import perf_counter

import bpy
import numpy as np
from blender_air import capture_linear
from diffuse_bounce import (
    REFLECTION_REACH_M,
    hemisphere_directions,
    hemisphere_rays,
    receiver_samples,
    reflection_index,
    write_irradiance,
)
from sky_environment import sample_radiance

ATTRIBUTE = "Street skylight"
TRACED = "Traced diffuse sky"
WEIGHT = "Traced sky weight"
SAMPLES = 256


def visible_radiance(tree, radiance, point, normal, directions):
    origin, rays = hemisphere_rays(point, normal, directions)
    open_rays = np.array(
        [
            tree.ray_cast(origin, direction, REFLECTION_REACH_M)[2] is None
            for direction in rays
        ]
    )
    # The cosine-weighted PDF cancels the Lambert cosine and 1/pi.
    return sample_radiance(radiance, rays[open_rays]).sum(axis=0) / len(rays)


def add_sky_passes(material):
    tree = material.node_tree
    field = tree.nodes.new("ShaderNodeVertexColor")
    field.layer_name = ATTRIBUTE
    traced = tree.nodes.new("ShaderNodeOutputAOV")
    traced.aov_name = TRACED
    tree.links.new(field.outputs["Color"], traced.inputs["Color"])
    weight = tree.nodes.new("ShaderNodeOutputAOV")
    weight.aov_name = WEIGHT
    tree.links.new(field.outputs["Alpha"], weight.inputs["Value"])


def bake_street_sky(scene, batches, radiance):
    start = perf_counter()
    eye = np.asarray(scene.camera.matrix_world.translation)
    samples = receiver_samples(batches, eye)
    if not len(samples.first):
        return 0, perf_counter() - start
    graph = bpy.context.evaluated_depsgraph_get()
    tree, _ = reflection_index(graph, eye, np.zeros(3))
    directions = hemisphere_directions(SAMPLES)
    values = np.array(
        [
            visible_radiance(
                tree, radiance, samples.positions[i], samples.normals[i], directions
            )
            for i in samples.first
        ]
    )
    materials = write_irradiance(
        batches, samples.expand(values), ATTRIBUTE, fade=samples.fade
    )
    variants = {material: material.copy() for material in materials}
    for material in variants.values():
        add_sky_passes(material)
    for mesh in {batch.mesh for batch in batches}:
        mesh.materials[0] = variants[mesh.materials[0]]
    scene.view_layers[0].use_pass_diffuse_color = True
    for name, kind in ((TRACED, "COLOR"), (WEIGHT, "VALUE")):
        aov = scene.view_layers[0].aovs.add()
        aov.name = name
        aov.type = kind
    bpy.context.view_layer.update()
    return len(values), perf_counter() - start


def combine(tree, op, left, right):
    node = tree.nodes.new("ShaderNodeMix")
    node.data_type = "RGBA"
    node.blend_type = op
    node.inputs[0].default_value = 1
    tree.links.new(left, node.inputs[6])
    tree.links.new(right, node.inputs[7])
    return node.outputs[2]


def capture_probe_diffuse(scene, sun):
    layer = scene.view_layers[0]
    original = (
        scene.compositing_node_group,
        layer.use_pass_diffuse_direct,
        layer.use_pass_diffuse_color,
        sun.energy,
    )
    tree = bpy.data.node_groups.new("Street probe diffuse sky", "CompositorNodeTree")
    try:
        scene.compositing_node_group = tree
        layer.use_pass_diffuse_direct = layer.use_pass_diffuse_color = True
        sun.energy = 0
        tree.interface.new_socket(
            name="Image", in_out="OUTPUT", socket_type="NodeSocketColor"
        )
        passes = tree.nodes.new("CompositorNodeRLayers").outputs
        diffuse = combine(
            tree, "MULTIPLY", passes["Diffuse Direct"], passes["Diffuse Color"]
        )
        masked = combine(tree, "MULTIPLY", diffuse, passes[WEIGHT])
        tree.links.new(masked, tree.nodes.new("NodeGroupOutput").inputs["Image"])
        return capture_linear(scene, "street-probe-diffuse", composite=True)
    finally:
        (
            scene.compositing_node_group,
            layer.use_pass_diffuse_direct,
            layer.use_pass_diffuse_color,
            sun.energy,
        ) = original
        bpy.data.node_groups.remove(tree)


def replace_probe_diffuse(tree, surface, *, captured, surface_filter):
    previous = tree.nodes.new("CompositorNodeImage")
    previous.image = captured
    colour = (
        surface_filter(tree, surface) if surface_filter else surface.outputs["Image"]
    )
    traced = combine(
        tree, "MULTIPLY", surface.outputs[TRACED], surface.outputs["Diffuse Color"]
    )
    corrected = combine(tree, "ADD", colour, traced)
    return combine(tree, "SUBTRACT", corrected, previous.outputs["Image"])
