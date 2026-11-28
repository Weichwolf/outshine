"""Replace the probe-based diffuse sky term at traced street receivers."""

from time import perf_counter

import bpy
import numpy as np
from blender_air import capture_linear
from diffuse_bounce import (
    REFLECTION_REACH_M,
    SURFACE_BIAS_M,
    hemisphere_basis,
    hemisphere_directions,
    hemisphere_rays,
    receiver_samples,
    write_irradiance,
)
from sky_environment import sample_radiance

ATTRIBUTE = "Street skylight"
TRACED = "Traced diffuse sky"
WEIGHT = "Traced sky weight"
SAMPLES = 256
LEAF_BIAS_M = 0.0015


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


def scattered_skylight(
    index, radiance, point, normal, directions, secondary, *, through_leaves
):
    tree = index.tree
    origin, rays = hemisphere_rays(point, normal, directions)
    escaped, scattered, colours = [], [], []
    for ray, (x, y, z) in zip(rays, secondary, strict=True):
        hit, surface, face, _ = tree.ray_cast(origin, ray, REFLECTION_REACH_M)
        if face is None:
            escaped.append(ray)
            continue
        material = index.face_materials[face]
        reflection = index.reflectance[material]
        transmission = index.transmittance[material] if through_leaves else None
        thin = transmission is not None and transmission.any()
        if not thin and not reflection.any():
            continue
        if np.dot(surface, ray) > 0:
            surface = -surface
        tangent, bitangent, surface = hemisphere_basis(surface)
        sideways = tangent * x + bitangent * y
        lobes = [(surface, reflection)]
        if thin:
            lobes.append((-surface, transmission))
        bias = LEAF_BIAS_M if thin else SURFACE_BIAS_M
        for side, colour in lobes:
            if not colour.any():
                continue
            bounce = sideways + side * z
            start = hit + side * bias
            if tree.ray_cast(start, bounce, REFLECTION_REACH_M)[2] is None:
                scattered.append(bounce)
                colours.append(colour)
    direct = sample_radiance(radiance, np.asarray(escaped).reshape(-1, 3))
    indirect = sample_radiance(radiance, np.asarray(scattered).reshape(-1, 3))
    scattered_radiance = indirect * np.asarray(colours).reshape(-1, 3)
    return (direct.sum(axis=0) + scattered_radiance.sum(axis=0)) / len(rays)


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


def bake_street_sky(
    scene, batches, radiance, *, index, below_deck=False, woodland=False
):
    start = perf_counter()
    eye = np.asarray(scene.camera.matrix_world.translation)
    samples = receiver_samples(batches, eye)
    if not len(samples.first):
        return 0, perf_counter() - start
    directions = hemisphere_directions(SAMPLES)
    if below_deck or woodland:
        order = (73 * np.arange(SAMPLES) + 53) % SAMPLES
        secondary = directions[order] * np.array([-1, -1, 1])
        values = [
            scattered_skylight(
                index,
                radiance,
                samples.positions[i],
                samples.normals[i],
                directions,
                secondary,
                through_leaves=woodland,
            )
            for i in samples.first
        ]
    else:
        values = [
            visible_radiance(
                index.tree,
                radiance,
                samples.positions[i],
                samples.normals[i],
                directions,
            )
            for i in samples.first
        ]
    materials = write_irradiance(
        batches, samples.expand(np.asarray(values)), ATTRIBUTE, fade=samples.fade
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
