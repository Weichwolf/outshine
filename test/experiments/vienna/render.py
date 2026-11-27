"""Render selected Vienna reference views in a single Blender GPU process."""

import argparse
import json
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path[:0] = [str(HERE), str(ROOT / "lib/src")]
sys.path.extend(
    map(str, (ROOT / "build/blender-python/lib").glob("python*/site-packages"))
)

from blender_air import configure_aerial
from catalog import available_views, load_catalog, selected_views
from ground_detail import (
    BARK_BOX_BLEND,
    DETAIL_SPAN_M,
    HEIGHT_SPAN_M,
    MACRO_SPAN_M,
    SURFACE_DETAIL,
    ground_detail,
)
from materials import DIFFUSE_TRANSMISSION, GLASS_IOR, METALLIC, SURFACES
from outshine_ref.rendering.blender_camera import configure_camera
from outshine_ref.rendering.blender_light import configure_daylight
from outshine_ref.rendering.blender_render import configure_gpu, render_png
from reference_scene import ReferenceScene

SUN_SHADOW_MIN_TEXEL_M = 0.016


def apply_detail(material, bsdf, name, colour):
    image_name = f"Outshine detail {name}"
    image = bpy.data.images.get(image_name)
    if image is None:
        pixels = ground_detail(name)
        size = pixels.shape[0]
        image = bpy.data.images.new(
            image_name, width=size, height=size, float_buffer=True
        )
        image.colorspace_settings.name = "Non-Color"
        image.alpha_mode = "CHANNEL_PACKED"
        image.pixels.foreach_set(pixels.ravel())
    nodes = material.node_tree.nodes
    link = material.node_tree.links.new
    position = nodes.new("ShaderNodeNewGeometry").outputs["Position"]
    texels = []
    for span in (DETAIL_SPAN_M, MACRO_SPAN_M):
        coords = nodes.new("ShaderNodeVectorMath")
        coords.operation = "SCALE"
        coords.inputs["Scale"].default_value = 1 / span
        link(position, coords.inputs[0])
        sampler = nodes.new("ShaderNodeTexImage")
        sampler.image = image
        sampler.projection = "BOX"
        if name.endswith(" bark"):
            sampler.projection_blend = BARK_BOX_BLEND
        link(coords.outputs["Vector"], sampler.inputs["Vector"])
        texels.append(sampler)
    detail, macro = texels
    channels = nodes.new("ShaderNodeSeparateColor")
    link(detail.outputs["Color"], channels.inputs["Color"])
    link(channels.outputs["Green"], bsdf.inputs["Roughness"])
    variance = nodes.new("ShaderNodeMath")
    variance.operation = "MULTIPLY_ADD"
    link(macro.outputs["Alpha"], variance.inputs[0])
    variance.inputs[1].default_value = 0.64
    variance.inputs[2].default_value = 1.68
    tint = nodes.new("ShaderNodeMath")
    tint.operation = "MULTIPLY"
    link(channels.outputs["Red"], tint.inputs[0])
    link(variance.outputs[0], tint.inputs[1])
    base = nodes.new("ShaderNodeVectorMath")
    base.operation = "SCALE"
    base.inputs[0].default_value = colour
    link(tint.outputs[0], base.inputs["Scale"])
    link(base.outputs["Vector"], bsdf.inputs["Base Color"])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Distance"].default_value = HEIGHT_SPAN_M
    link(channels.outputs["Blue"], bump.inputs["Height"])
    link(bump.outputs["Normal"], bsdf.inputs["Normal"])


def surface_material(name):
    material = bpy.data.materials.get(name)
    if material:
        return material
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    bsdf = material.node_tree.nodes["Principled BSDF"]
    colour, roughness = SURFACES[name]
    bsdf.inputs["Base Color"].default_value = (*colour, 1)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = METALLIC.get(name, 0.0)
    if name == "glass":
        bsdf.inputs["IOR"].default_value = GLASS_IOR
    if detail := SURFACE_DETAIL.get(name):
        apply_detail(material, bsdf, detail, colour)
    if transmission := DIFFUSE_TRANSMISSION.get(name):
        tree = material.node_tree
        backlight = tree.nodes.new("ShaderNodeBsdfTranslucent")
        backlight.inputs["Color"].default_value = (*colour, 1)
        mix = tree.nodes.new("ShaderNodeMixShader")
        mix.inputs[0].default_value = transmission
        tree.links.new(bsdf.outputs[0], mix.inputs[1])
        tree.links.new(backlight.outputs[0], mix.inputs[2])
        tree.links.new(mix.outputs[0], tree.nodes["Material Output"].inputs["Surface"])
    return material


def upload(name, vertices, faces, *, collection, smooth=False):
    mesh = bpy.data.meshes.new(name)
    if isinstance(vertices, np.ndarray) and isinstance(faces, np.ndarray):
        used, indices = np.unique(faces, return_inverse=True)
        mesh.vertices.add(len(used))
        mesh.vertices.foreach_set("co", np.asarray(vertices[used], np.float32).ravel())
        mesh.loops.add(faces.size)
        mesh.loops.foreach_set("vertex_index", np.asarray(indices, np.int32).ravel())
        mesh.polygons.add(len(faces))
        mesh.polygons.foreach_set(
            "loop_start", np.arange(0, faces.size, faces.shape[1], dtype=np.int32)
        )
    else:
        mesh.from_pydata(vertices, [], faces)
    if smooth:
        mesh.polygons.foreach_set("use_smooth", np.ones(len(mesh.polygons), dtype=bool))
    mesh.update()
    mesh.materials.append(surface_material(name))
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    return obj


def plant_woodland(world, seed, geometry):
    count = 0
    for species, group, meshes, placements in world.trees(seed):
        if group is None:
            for name, (vertices, faces) in meshes.items():
                upload(name, vertices, faces, collection=geometry, smooth=True)
            continue
        collection = bpy.data.collections.new(f"{species} group {group}")
        for name, (vertices, faces) in meshes.items():
            upload(
                name,
                vertices,
                faces,
                collection=collection,
                smooth=name.endswith(" bark"),
            )
        for x, y, z, angle, scale in placements:
            instance = bpy.data.objects.new(species, None)
            instance.instance_type = "COLLECTION"
            instance.instance_collection = collection
            instance.location = (x, y, z)
            instance.rotation_euler.z = angle
            instance.scale = (scale,) * 3
            geometry.objects.link(instance)
        count += len(placements)
    return count


def render(place, config, args):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    device = configure_gpu(scene, "eevee")
    configure_daylight(scene)
    world = ReferenceScene(place, config)
    sunlight = Vector(world.sun)
    sun = next(o for o in scene.objects if o.type == "LIGHT" and o.data.type == "SUN")
    sun.rotation_euler = (-sunlight).to_track_quat("-Z", "Y").to_euler()
    sun.data.shadow_maximum_resolution = SUN_SHADOW_MIN_TEXEL_M
    sky = scene.world.node_tree.nodes.new("ShaderNodeTexSky")
    sky.sky_type = "HOSEK_WILKIE"
    sky.sun_direction = sunlight
    sky.turbidity = 2.2
    background = scene.world.node_tree.nodes["Background"]
    background.inputs["Strength"].default_value = 0.25
    scene.world.node_tree.links.new(sky.outputs["Color"], background.inputs["Color"])
    scene.cycles.seed = config["seed"]
    geometry = bpy.data.collections.new("World geometry")
    scene.collection.children.link(geometry)
    for name, verts, faces in world.surfaces():
        upload(name, verts, faces, collection=geometry)
    for name, verts, faces in world.grass_and_understory(config["seed"]):
        upload(name, verts, faces, collection=geometry)
    eye, look = Vector(world.eye), Vector(world.look)
    scene.camera = configure_camera(
        {
            "eye": eye,
            "target": eye + look,
            "projection": "perspective",
            "vertical_fov_deg": world.fov,
            "clip_near_m": 0.05,
            "clip_far_m": 240000,
        }
    )
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    scene.render.resolution_percentage = 100
    trees = plant_woodland(world, config["seed"], geometry)
    scene.eevee.taa_render_samples = 16
    configure_aerial(scene, geometry, eye.z + world.source.datum)
    if args.engine == "cycles":
        device = configure_gpu(scene, "cycles")
    image = args.output / (place["id"] + ".png")
    args.output.mkdir(parents=True, exist_ok=True)
    times = [render_png(scene, image) for _ in range(2 if args.repeat else 1)]
    print(
        json.dumps(
            {
                "place": place["id"],
                "surface": world.support,
                "woodland_trees": trees,
                "eye_m": eye.z + world.source.datum,
                **device,
                "seconds_to_png": times,
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    config = load_catalog()
    p = argparse.ArgumentParser()
    p.add_argument(
        "--place", action="append", choices=[v["id"] for v in available_views(config)]
    )
    p.add_argument("--output", type=Path, default=ROOT / "build/shots/places")
    p.add_argument("--engine", choices=["eevee", "cycles"], default="eevee")
    p.add_argument("--repeat", action="store_true")
    args = p.parse_args(
        sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    )
    for place in selected_views(config, args.place):
        render(place, config, args)
