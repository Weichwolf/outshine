"""Apply clear-air extinction to the scene-linear Blender image."""

import math
from pathlib import Path
from tempfile import TemporaryDirectory
from time import perf_counter

import bpy


def capture_sky(scene, ground):
    settings = scene.render
    image_settings = settings.image_settings
    original = (
        image_settings.file_format,
        image_settings.color_mode,
        image_settings.color_depth,
        settings.filepath,
        settings.use_compositing,
        ground.use,
    )
    try:
        ground.use = False
        settings.use_compositing = False
        image_settings.file_format = "OPEN_EXR"
        image_settings.color_mode = "RGBA"
        image_settings.color_depth = "32"
        with TemporaryDirectory(prefix="outshine-vienna-sky-") as tmp:
            settings.filepath = str(Path(tmp) / "sky.exr")
            if "FINISHED" not in bpy.ops.render.render(
                write_still=True, scene=scene.name
            ):
                raise RuntimeError("EEVEE sky capture did not finish")
            image = bpy.data.images.load(settings.filepath)
            image.pack()
            return image
    finally:
        (
            image_settings.file_format,
            image_settings.color_mode,
            image_settings.color_depth,
            settings.filepath,
            settings.use_compositing,
            ground.use,
        ) = original


def configure_aerial(scene, geometry, eye_altitude_m, *, cache_sky=False):
    ground = scene.view_layers[0]
    ground.use_pass_mist = True
    sky = scene.view_layers.new("Clear sky")
    sky.layer_collection.children[geometry.name].exclude = True
    sky_image, sky_seconds = None, 0.0
    if cache_sky:
        start = perf_counter()
        sky_image = capture_sky(scene, ground)
        scene.view_layers.remove(sky)
        sky_seconds = perf_counter() - start
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
    if sky_image is None:
        airlight = tree.nodes.new("CompositorNodeRLayers")
        airlight.layer = sky.name
    else:
        airlight = tree.nodes.new("CompositorNodeImage")
        airlight.image = sky_image

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
    return sky_seconds
