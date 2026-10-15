"""Actual fixed Place camera/solar clock and simple procedural scene materials."""

from datetime import datetime

import bpy
import numpy as np
from astral import Observer
from astral.sun import azimuth, elevation
from blender_preview import material
from mathutils import Matrix, Vector
from organ_preview import link_shadow_products
from outshine_ref.places import axes
from preview_lighting import SUN_ANGULAR_DIAMETER_DEG
from preview_shadows import configure_preview


def sun_direction(camera):
    observer = Observer(camera.latitude_deg, camera.longitude_deg)
    clock = datetime.fromisoformat(camera.time_utc)
    az, el = np.deg2rad((azimuth(observer, clock), elevation(observer, clock)))
    local = np.array((np.sin(az) * np.cos(el), np.cos(az) * np.cos(el), np.sin(el)))
    direction = (
        axes(camera.origin_latitude_deg, camera.origin_longitude_deg)
        @ axes(camera.latitude_deg, camera.longitude_deg).T
        @ local
    )
    return direction, {
        "azimuth_deg": float(np.rad2deg(az)),
        "elevation_deg": float(np.rad2deg(el)),
        "clock": camera.time_utc,
    }


def setup(camera, ground, profile, sun):
    scene = bpy.context.scene
    configure_preview(scene)
    scene.render.resolution_x, scene.render.resolution_y = profile
    scene.render.resolution_percentage = 100
    scene.eevee.taa_render_samples = 64
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.view_transform = "AgX"
    data = bpy.data.cameras.new(camera.name)
    data.sensor_fit, data.sensor_height = "VERTICAL", 32
    data.lens = 32 / (2 * np.tan(np.deg2rad(camera.vertical_fov_deg) / 2))
    data.clip_start, data.clip_end = 0.1, camera.sight_m
    obj = bpy.data.objects.new(camera.name, data)
    bpy.context.collection.objects.link(obj)
    transform = np.eye(4)
    transform[:3, :3], transform[:3, 3] = camera.basis, camera.eye(ground)
    obj.matrix_world = Matrix(transform.tolist())
    scene.camera = obj
    light = bpy.data.lights.new("actual Place sun", "SUN")
    light.energy, light.angle = 3.2, np.deg2rad(SUN_ANGULAR_DIAMETER_DEG)
    lamp = bpy.data.objects.new("sun", light)
    bpy.context.collection.objects.link(lamp)
    lamp.rotation_euler = Vector((-sun).tolist()).to_track_quat("-Z", "Y").to_euler()
    link_shadow_products(lamp, "sun")
    world = bpy.data.worlds.new("procedural daylight")
    world.use_nodes = True
    scene.world = world
    nodes, links = world.node_tree.nodes, world.node_tree.links
    sky = nodes.new("ShaderNodeTexSky")
    sky.sky_type, sky.sun_disc = "MULTIPLE_SCATTERING", False
    sky.sun_elevation = float(np.arcsin(sun[2]))
    sky.sun_rotation = float(np.arctan2(sun[1], sun[0]))
    links.new(sky.outputs[0], nodes["Background"].inputs["Color"])
    nodes["Background"].inputs["Strength"].default_value = 0.3
    return scene


def ground_material():
    result = material("procedural grass and exposed rock", (0.12, 0.19, 0.055), 0.95)
    nodes, links = result.node_tree.nodes, result.node_tree.links
    geometry = nodes.new("ShaderNodeNewGeometry")
    slope = nodes.new("ShaderNodeVectorMath")
    slope.operation = "DOT_PRODUCT"
    slope.inputs[1].default_value = (0, 0, 1)
    links.new(geometry.outputs["Normal"], slope.inputs[0])
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.55
    ramp.color_ramp.elements[0].color = (0.24, 0.23, 0.21, 1)
    ramp.color_ramp.elements[1].position = 0.9
    ramp.color_ramp.elements[1].color = (0.13, 0.20, 0.055, 1)
    links.new(slope.outputs["Value"], ramp.inputs["Fac"])
    meadow = nodes.new("ShaderNodeTexNoise")
    meadow.inputs["Scale"].default_value = 0.18
    meadow.inputs["Detail"].default_value = 3
    links.new(geometry.outputs["Position"], meadow.inputs["Vector"])
    tones = nodes.new("ShaderNodeValToRGB")
    tones.color_ramp.elements[0].color = (0.35, 0.44, 0.15, 1)
    tones.color_ramp.elements[1].color = (0.95, 0.86, 0.46, 1)
    links.new(meadow.outputs["Fac"], tones.inputs["Fac"])
    colour = nodes.new("ShaderNodeMixRGB")
    colour.blend_type = "MULTIPLY"
    colour.inputs[0].default_value = 1
    links.new(ramp.outputs["Color"], colour.inputs[1])
    links.new(tones.outputs["Color"], colour.inputs[2])
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 0.12
    links.new(geometry.outputs["Position"], noise.inputs["Vector"])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.2
    bump.inputs["Distance"].default_value = 0.05
    links.new(noise.outputs["Fac"], bump.inputs["Height"])
    shader = nodes["Principled BSDF"]
    links.new(bump.outputs[0], shader.inputs["Normal"])
    links.new(colour.outputs["Color"], shader.inputs["Base Color"])
    return result
