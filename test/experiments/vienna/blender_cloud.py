"""Attenuate the direct Sun in a fixed view using its linear radiance difference."""

from contextlib import contextmanager

import bpy
import numpy as np
from blender_air import capture_linear, math_node
from cloud_profile import SUN_MAP_RADIUS_KM


class CloudLighting:
    def __init__(self, world, sun, transmission):
        self.world = world
        self.sun = sun
        height, width = transmission.shape
        image = bpy.data.images.new(
            "Cloud transmission", width, height, float_buffer=True
        )
        image.colorspace_settings.name = "Non-Color"
        pixels = np.ones((height, width, 4), dtype="f4")
        pixels[..., :3] = transmission[..., None]
        image.pixels.foreach_set(pixels.ravel())
        self.image = image

    def capture_ambient(self, scene):
        energy = self.sun.energy
        try:
            self.sun.energy = 0
            return capture_linear(scene, "cloud-ambient")
        finally:
            self.sun.energy = energy

    def mix(self, tree, surface, *, ambient):
        world = self.world
        origin = world.clouds.sun_map_origin(world.eye, world.source.datum)
        direction = world.sun[:2] / max(world.sun[2], 0.01)
        scale = 1e-3 / (2 * SUN_MAP_RADIUS_KM)

        position = tree.nodes.new("ShaderNodeSeparateXYZ")
        tree.links.new(surface.outputs["Position"], position.inputs[0])
        height = math_node(tree, "SUBTRACT", position.outputs["Z"], origin[2])
        uv = tree.nodes.new("ShaderNodeCombineXYZ")
        uv.inputs["Z"].default_value = 1
        for i, axis in enumerate(("X", "Y")):
            drift = math_node(tree, "MULTIPLY", height, direction[i])
            projected = math_node(tree, "SUBTRACT", position.outputs[axis], drift)
            mapped = math_node(
                tree, "MULTIPLY_ADD", projected, scale, 0.5 - origin[i] * scale
            )
            margin = 0.5 / self.image.size[i]
            inner = math_node(tree, "MAXIMUM", mapped, margin)
            clamped = math_node(tree, "MINIMUM", inner, 1 - margin)
            tree.links.new(clamped, uv.inputs[axis])

        texture = tree.nodes.new("CompositorNodeImage")
        texture.image = self.image
        lookup = tree.nodes.new("CompositorNodeMapUV")
        tree.links.new(texture.outputs["Image"], lookup.inputs["Image"])
        tree.links.new(uv.outputs["Vector"], lookup.inputs["UV"])
        top_m = world.clouds.layer[1] * 1000 - world.source.datum
        above = math_node(tree, "GREATER_THAN", position.outputs["Z"], top_m)
        transmission = math_node(tree, "MAXIMUM", lookup.outputs["Image"], above)

        indirect = tree.nodes.new("CompositorNodeImage")
        indirect.image = ambient
        mix = tree.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        tree.links.new(transmission, mix.inputs[0])
        tree.links.new(indirect.outputs["Image"], mix.inputs[6])
        tree.links.new(surface.outputs["Image"], mix.inputs[7])
        return mix.outputs[2]

    @contextmanager
    def reflection(self, scene):
        ambient = self.capture_ambient(scene)
        layer = scene.view_layers[0]
        original = (
            scene.compositing_node_group,
            scene.render.use_compositing,
            layer.use_pass_position,
        )
        tree = bpy.data.node_groups.new(
            "Cloud light in reflection", "CompositorNodeTree"
        )
        try:
            scene.compositing_node_group = tree
            scene.render.use_compositing = True
            layer.use_pass_position = True
            tree.interface.new_socket(
                name="Image", in_out="OUTPUT", socket_type="NodeSocketColor"
            )
            surface = tree.nodes.new("CompositorNodeRLayers")
            surface.layer = layer.name
            colour = self.mix(tree, surface, ambient=ambient)
            tree.links.new(colour, tree.nodes.new("NodeGroupOutput").inputs["Image"])
            yield
        finally:
            (
                scene.compositing_node_group,
                scene.render.use_compositing,
                layer.use_pass_position,
            ) = original
            bpy.data.node_groups.remove(tree)
            bpy.data.images.remove(ambient)
