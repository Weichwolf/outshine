"""Composite atmospheric extinction and airlight using the render's distance pass."""

import math
from pathlib import Path
from tempfile import TemporaryDirectory
from time import perf_counter

import bpy
import numpy as np
from air_profile import (
    AEROSOL_COLUMN_HEIGHT_KM,
    AEROSOL_HEIGHT_KM,
    AEROSOL_SAMPLES,
    EARTH_RADIUS_KM,
)
from water_reflection import camera_axes


def capture_linear(scene, name):
    settings = scene.render
    image_settings = settings.image_settings
    original = (
        image_settings.file_format,
        image_settings.color_mode,
        image_settings.color_depth,
        settings.filepath,
        settings.use_compositing,
        image_settings.exr_codec,
    )
    try:
        settings.use_compositing = False
        image_settings.file_format = "OPEN_EXR"
        image_settings.color_mode = "RGBA"
        image_settings.color_depth = "32"
        image_settings.exr_codec = "NONE"
        with TemporaryDirectory(prefix="outshine-vienna-light-") as tmp:
            settings.filepath = str(Path(tmp) / f"{name}.exr")
            if "FINISHED" not in bpy.ops.render.render(
                write_still=True, scene=scene.name
            ):
                raise RuntimeError(f"EEVEE {name} capture did not finish")
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
            image_settings.exr_codec,
        ) = original


def capture_sky(scene, ground):
    enabled = ground.use
    try:
        ground.use = False
        return capture_linear(scene, "sky")
    finally:
        ground.use = enabled


def math_node(tree, op, *inputs):
    node = tree.nodes.new("ShaderNodeMath")
    node.operation = op
    for socket, source in zip(node.inputs, inputs):
        if isinstance(source, bpy.types.NodeSocket):
            tree.links.new(source, socket)
        else:
            socket.default_value = float(source)
    return node.outputs[0]


def zenith_rays(tree, scene, world):
    width, height = scene.render.resolution_x, scene.render.resolution_y
    right, up = camera_axes(world.look)
    x = (np.arange(width) + 0.5 - width / 2) / world.focal
    y = (np.arange(height) + 0.5 - height / 2)[:, None] / world.focal
    vertical = (world.look[2] + x * right[2] + y * up[2]) / np.sqrt(1 + x * x + y * y)
    image = bpy.data.images.new("Ray zenith", width, height, float_buffer=True)
    image.colorspace_settings.name = "Non-Color"
    pixels = np.ones((height, width, 4), dtype="f4")
    pixels[..., :3] = 0.5 + 0.5 * vertical[..., None]
    image.pixels.foreach_set(pixels.ravel())
    rays = tree.nodes.new("CompositorNodeImage")
    rays.image = image
    return math_node(tree, "MULTIPLY_ADD", rays.outputs["Image"], 2, -1)


def aerosol_depth(tree, scene, world, eye_m, distance):
    vertical = zenith_rays(tree, scene, world)
    above = math_node(tree, "MAXIMUM", vertical, 1e-5)
    cutoff = math_node(tree, "DIVIDE", AEROSOL_COLUMN_HEIGHT_KM, above)
    distance = math_node(tree, "MINIMUM", distance, cutoff)
    # Radial rise to second order in path length over the atmosphere's radius.
    horizontal_squared = math_node(
        tree, "MULTIPLY_ADD", vertical, math_node(tree, "MULTIPLY", vertical, -1), 1
    )
    curvature = math_node(
        tree, "MULTIPLY", horizontal_squared, 0.5 / (EARTH_RADIUS_KM + eye_m * 0.001)
    )
    column = 0.0
    for fraction, weight in AEROSOL_SAMPLES:
        s = math_node(tree, "MULTIPLY", distance, fraction)
        slope = math_node(tree, "MULTIPLY_ADD", s, curvature, vertical)
        altitude = math_node(tree, "MULTIPLY_ADD", s, slope, eye_m * 0.001)
        height = math_node(tree, "MAXIMUM", altitude, 0)
        density = math_node(
            tree,
            "EXPONENT",
            math_node(tree, "MULTIPLY", height, -1 / AEROSOL_HEIGHT_KM),
        )
        column = math_node(tree, "MULTIPLY_ADD", density, weight, column)
    return math_node(
        tree,
        "MULTIPLY",
        math_node(tree, "MULTIPLY", distance, column),
        -world.aerosol_extinction,
    )


def clear_air_depth(eye_m, distance_m):
    return -0.013558 * math.exp(-max(0, eye_m) / 8000) * distance_m * 0.001


def configure_aerial(
    scene, geometry, world, eye_altitude_m, *, cache_sky=False, surface_filter=None
):
    ground = scene.view_layers[0]
    ground.use_pass_mist = True
    if surface_filter is not None:
        ground.use_pass_position = True
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
    colour = surface.outputs["Image"]
    if surface_filter is not None:
        colour = surface_filter(tree, surface)
    if sky_image is None:
        airlight = tree.nodes.new("CompositorNodeRLayers")
        airlight.layer = sky.name
    else:
        airlight = tree.nodes.new("CompositorNodeImage")
        airlight.image = sky_image

    mist_distance = surface.outputs["Mist"]
    optical_depth = math_node(
        tree, "MULTIPLY", mist_distance, clear_air_depth(eye_altitude_m, mist.depth)
    )
    optical_depth.node.name = "Clear-air optical depth"
    if world.aerosol_extinction:
        distance = math_node(tree, "MULTIPLY", mist_distance, mist.depth * 0.001)
        aerosol = aerosol_depth(tree, scene, world, eye_altitude_m, distance)
        optical_depth = math_node(tree, "ADD", optical_depth, aerosol)
    transmission = math_node(tree, "EXPONENT", optical_depth)

    mix = tree.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    tree.links.new(transmission, mix.inputs[0])
    tree.links.new(airlight.outputs["Image"], mix.inputs[6])
    tree.links.new(colour, mix.inputs[7])
    tree.links.new(mix.outputs[2], tree.nodes.new("NodeGroupOutput").inputs["Image"])
    return sky_seconds
