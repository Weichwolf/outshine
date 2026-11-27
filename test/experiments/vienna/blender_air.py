"""Apply clear-air extinction to the scene-linear Blender image."""

import math

import bpy


def configure_aerial(scene, geometry, eye_altitude_m):
    ground = scene.view_layers[0]
    ground.use_pass_mist = True
    sky = scene.view_layers.new("Clear sky")
    sky.layer_collection.children[geometry.name].exclude = True
    mist = scene.world.mist_settings
    mist.start = 0
    mist.depth = scene.camera.data.clip_end
    mist.falloff = "LINEAR"

    tree = bpy.data.node_groups.new("Clear air", "CompositorNodeTree")
    tree.interface.new_socket(
        name="Image", in_out="OUTPUT", socket_type="NodeSocketColor"
    )
    scene.compositing_node_group = tree
    surface = tree.nodes.new("CompositorNodeRLayers")
    surface.layer = ground.name
    airlight = tree.nodes.new("CompositorNodeRLayers")
    airlight.layer = sky.name

    density = math.exp(-max(0, eye_altitude_m) / 8000)
    optical_depth = tree.nodes.new("ShaderNodeMath")
    optical_depth.operation = "MULTIPLY"
    optical_depth.inputs[1].default_value = -0.013558 * density * mist.depth * 0.001
    tree.links.new(surface.outputs["Mist"], optical_depth.inputs[0])
    transmission = tree.nodes.new("ShaderNodeMath")
    transmission.operation = "EXPONENT"
    tree.links.new(optical_depth.outputs[0], transmission.inputs[0])

    mix = tree.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    tree.links.new(transmission.outputs[0], mix.inputs[0])
    tree.links.new(airlight.outputs["Image"], mix.inputs[6])
    tree.links.new(surface.outputs["Image"], mix.inputs[7])
    tree.links.new(mix.outputs[2], tree.nodes.new("NodeGroupOutput").inputs["Image"])
