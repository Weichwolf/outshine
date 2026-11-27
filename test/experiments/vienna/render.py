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
from blender_reflection import apply_reflection, capture_images
from catalog import available_views, load_catalog, selected_views
from ground_detail import (
    BARK_BOX_BLEND,
    DETAIL_SPAN_M,
    DETAIL_TINTS,
    HEIGHT_SPAN_M,
    MACRO_SPAN_M,
    SURFACE_DETAIL,
    ground_detail,
)
from materials import (
    DIFFUSE_TRANSMISSION,
    GLASS_IOR,
    METALLIC,
    SURFACES,
    WATER_IOR,
    WATER_MATERIALS,
    surface_properties,
)
from outshine_ref.rendering.blender_camera import configure_camera
from outshine_ref.rendering.blender_light import (
    configure_daylight,
    configure_research_view,
)
from outshine_ref.rendering.blender_render import configure_gpu, render_png
from passage_light import bake_passage_lighting
from reference_scene import ReferenceScene
from sky_environment import environment_path, load_environment
from water_bed import BED_ALBEDO
from water_reflection import reflection_planes

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
    reflectance = base.outputs["Vector"]
    if pigment_tint := DETAIL_TINTS.get(name):
        pigment = nodes.new("ShaderNodeMixRGB")
        pigment.blend_type = "MULTIPLY"
        pigment.inputs[2].default_value = (*pigment_tint, 1)
        link(detail.outputs["Alpha"], pigment.inputs[0])
        link(reflectance, pigment.inputs[1])
        reflectance = pigment.outputs["Color"]
    link(reflectance, bsdf.inputs["Base Color"])
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
    base, colour, roughness = surface_properties(name)
    bsdf.inputs["Base Color"].default_value = (*colour, 1)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = METALLIC.get(name, 0.0)
    if name == "glass":
        bsdf.inputs["IOR"].default_value = GLASS_IOR
    if name in WATER_MATERIALS:
        bsdf.inputs["IOR"].default_value = WATER_IOR
    if detail := SURFACE_DETAIL.get(base):
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


def apply_water_bed(bed):
    # The raster fades to zero at the boundary of the local water mesh.
    material = bpy.data.materials.get("water")
    if material is None:
        return
    height, width = bed.transmission.shape[:2]
    image = bpy.data.images.new(
        "Water bed transmission", width, height, float_buffer=False
    )
    image.colorspace_settings.name = "Non-Color"
    pixels = np.ones((height, width, 4), dtype="f4")
    pixels[..., :3] = bed.transmission
    image.pixels.foreach_set(pixels.ravel())
    east, north, inv_width, inv_height = bed.transform
    tree = material.node_tree
    nodes, link = tree.nodes, tree.links.new
    position = nodes.new("ShaderNodeNewGeometry").outputs["Position"]
    coords = nodes.new("ShaderNodeVectorMath")
    coords.operation = "MULTIPLY_ADD"
    coords.inputs[1].default_value = (inv_width, inv_height, 0)
    coords.inputs[2].default_value = (-east * inv_width, -north * inv_height, 0)
    link(position, coords.inputs[0])
    texture = nodes.new("ShaderNodeTexImage")
    texture.image = image
    texture.extension = "CLIP"
    link(coords.outputs["Vector"], texture.inputs["Vector"])
    colour = nodes.new("ShaderNodeVectorMath")
    colour.operation = "MULTIPLY_ADD"
    water = np.asarray(SURFACES["water"][0])
    colour.inputs[1].default_value = np.asarray(BED_ALBEDO) - water
    colour.inputs[2].default_value = water
    link(texture.outputs["Color"], colour.inputs[0])
    link(colour.outputs["Vector"], nodes["Principled BSDF"].inputs["Base Color"])


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


def place_woody_plants(world, seed, geometry):
    count = 0
    for species, group, meshes, placements in world.woody_plants(seed):
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


def configure_world_sky(scene, sunlight, radiance):
    nodes = scene.world.node_tree.nodes
    background = nodes["Background"]
    if radiance is None:
        sky = nodes.new("ShaderNodeTexSky")
        sky.sky_type = "HOSEK_WILKIE"
        sky.sun_direction = sunlight
        sky.turbidity = 2.2
        background.inputs["Strength"].default_value = 0.25
    else:
        height, width, _ = radiance.shape
        image = bpy.data.images.new(
            "Outshine sky radiance", width, height, alpha=False, float_buffer=True
        )
        image.colorspace_settings.name = "Non-Color"
        rgba = np.ones((height, width, 4), dtype="f4")
        rgba[:, :, :3] = radiance
        image.pixels.foreach_set(rgba.ravel())
        sky = nodes.new("ShaderNodeTexEnvironment")
        sky.projection = "EQUIRECTANGULAR"
        sky.image = image
        background.inputs["Strength"].default_value = 1.0
        scene.world.sun_threshold = 0.0
    scene.world.node_tree.links.new(sky.outputs["Color"], background.inputs["Color"])


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
    sky_dir = getattr(args, "sky_dir", None)
    sky_path = environment_path(sky_dir, place) if sky_dir is not None else None
    eye_m = world.eye[2] + world.source.datum
    radiance = (
        load_environment(sky_path, world.sun, eye_m) if sky_path is not None else None
    )
    configure_world_sky(scene, sunlight, radiance)
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
    scene.render.image_settings.compression = 0
    woody_instances = place_woody_plants(world, config["seed"], geometry)
    if world.water_bed is not None:
        apply_water_bed(world.water_bed)
    scene.eevee.taa_render_samples = 16
    probes, bake_s = (
        bake_passage_lighting(scene, world.local.covered_walks, world.eye[:2])
        if args.engine == "eevee"
        else (0, 0.0)
    )
    planes = reflection_planes(world) if args.engine == "eevee" else ()
    capture_s = 0.0
    if planes:
        images, capture_s = capture_images(scene, geometry, planes)
        for name in WATER_MATERIALS:
            if material := bpy.data.materials.get(name):
                apply_reflection(material, world.eye, planes, images)
    sky_s = configure_aerial(
        scene,
        geometry,
        eye_m,
        cache_sky=args.engine == "eevee" and args.repeat,
    )
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
                "woody_instances": woody_instances,
                "eye_m": eye_m,
                **device,
                "sky_radiance": str(sky_path) if sky_path is not None else None,
                "passage_irradiance_volumes": probes,
                "passage_irradiance_bake_s": round(bake_s, 3),
                "reflection_levels_m": [round(plane.height, 3) for plane in planes],
                "reflection_capture_s": round(capture_s, 3),
                "sky_capture_s": round(sky_s, 3),
                "seconds_to_png": times,
            }
        ),
        flush=True,
    )


def display_linear(paths, output):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    view = configure_research_view(scene)
    settings = scene.render.image_settings
    settings.file_format = "PNG"
    settings.color_mode = "RGB"
    settings.color_depth = "16"
    settings.compression = 0
    output.mkdir(parents=True, exist_ok=True)
    for path in paths:
        with path.open("rb") as source:
            rgb = np.load(source, allow_pickle=False)
        if (
            not isinstance(rgb, np.ndarray)
            or rgb.ndim != 3
            or rgb.shape[2] != 3
            or not rgb.size
            or rgb.dtype.kind != "f"
            or rgb.dtype.itemsize != 4
            or not np.isfinite(rgb).all()
        ):
            raise ValueError(
                f"Expected finite scene-linear float32 RGB pixels in {path}"
            )
        height, width, _ = rgb.shape
        rgba = np.ones((height, width, 4), dtype="f4")
        rgba[:, :, :3] = rgb[::-1]
        image = bpy.data.images.new(
            path.stem, width, height, alpha=True, float_buffer=True
        )
        try:
            image.colorspace_settings.name = "Linear Rec.709"
            image.pixels.foreach_set(rgba.ravel())
            destination = (output / path.with_suffix(".png").name).resolve()
            image.save_render(str(destination), scene=scene)
        finally:
            bpy.data.images.remove(image)
        print(
            json.dumps({"linear": str(path), "png": str(destination), **view}),
            flush=True,
        )


if __name__ == "__main__":
    config = load_catalog()
    p = argparse.ArgumentParser()
    inputs = p.add_mutually_exclusive_group()
    inputs.add_argument(
        "--place", action="append", choices=[v["id"] for v in available_views(config)]
    )
    inputs.add_argument(
        "--display-linear",
        type=Path,
        nargs="+",
        help="apply the Blender view to RGB NPY images",
    )
    p.add_argument("--output", type=Path)
    p.add_argument("--engine", choices=["eevee", "cycles"], default="eevee")
    p.add_argument(
        "--sky-dir", type=Path, help="use exported scene-linear sky radiance"
    )
    p.add_argument("--repeat", action="store_true")
    args = p.parse_args(
        sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    )
    if args.display_linear is not None:
        display_linear(
            args.display_linear, args.output or ROOT / "build/shots/glsl-agx"
        )
    else:
        args.output = args.output or ROOT / "build/shots/places"
        for place in selected_views(config, args.place):
            render(place, config, args)
