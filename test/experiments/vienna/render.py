"""Render selected Vienna reference views in a single Blender GPU process."""

import argparse
import json
import sys
from functools import partial
from pathlib import Path
from tempfile import TemporaryDirectory
from time import perf_counter
from types import SimpleNamespace

import bpy
import numpy as np
from mathutils import Vector

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path[:0] = [str(HERE), str(ROOT / "lib/src")]
sys.path.extend(
    map(str, (ROOT / "build/blender-python/lib").glob("python*/site-packages"))
)

from blender_air import clear_air_depth, configure_aerial
from blender_cloud import CloudLighting
from blender_reflection import apply_reflection, capture_images
from catalog import available_views, load_catalog, selected_views
from diffuse_bounce import bake_sun_bounce, street_receivers
from ground_detail import (
    BARK_BOX_BLEND,
    CANOPY_DETAIL,
    DETAIL_TINTS,
    SURFACE_DETAIL,
    detail_scale,
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
from passage_light import bake_reference_irradiance
from reference_scene import ReferenceScene
from sky_environment import environment_path, load_environment
from walk_reference import plan_walk
from water_bed import BED_ALBEDO
from water_reflection import reflection_planes

SUN_SHADOW_MIN_TEXEL_M = 0.016
CYCLES_REFERENCE_SAMPLES = 128
CYCLES_NOISE_THRESHOLD = 0.025


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
    planar = name == CANOPY_DETAIL
    if planar:
        axes = nodes.new("ShaderNodeVectorRotate")
        axes.rotation_type = "Z_AXIS"
        axes.inputs["Angle"].default_value = np.pi / 2
        link(position, axes.inputs["Vector"])
        position = axes.outputs["Vector"]
    fine_span, macro_span, height_span = detail_scale(name)
    texels = []
    for span in (fine_span, macro_span):
        coords = nodes.new("ShaderNodeVectorMath")
        coords.operation = "SCALE"
        coords.inputs["Scale"].default_value = 1 / span
        link(position, coords.inputs[0])
        sampler = nodes.new("ShaderNodeTexImage")
        sampler.image = image
        sampler.projection = "FLAT" if planar else "BOX"
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
    bump.inputs["Distance"].default_value = height_span
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


def place_woody_plants(world, seed, geometry, *, eye_route=None):
    count = 0
    for species, group, meshes, placements in world.woody_plants(
        seed, eye_route=eye_route
    ):
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


def configure_world_sky(scene, sunlight, radiance, *, haze=False):
    nodes = scene.world.node_tree.nodes
    background = nodes["Background"]
    image = None
    if radiance is None:
        sky = nodes.new("ShaderNodeTexSky")
        sky.sky_type = "HOSEK_WILKIE"
        sky.sun_direction = sunlight
        sky.turbidity = 4.0 if haze else 2.2
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
    return image


def configure_cycles_reference(scene, samples, repeat):
    device = configure_gpu(scene, "cycles")
    scene.cycles.samples = samples
    scene.cycles.use_adaptive_sampling = True
    scene.cycles.adaptive_threshold = CYCLES_NOISE_THRESHOLD
    scene.cycles.adaptive_min_samples = min(8, samples)
    scene.cycles.use_denoising = True
    scene.cycles.denoising_use_gpu = True
    scene.render.use_persistent_data = repeat
    return device


def render_walk(scene, world, walk, output, metadata):
    output.mkdir(parents=True, exist_ok=True)
    camera = scene.camera
    optical_depth = scene.compositing_node_group.nodes["Clear-air optical depth"]
    frames = []
    with TemporaryDirectory(prefix=".frames-", dir=output) as directory:
        pending = Path(directory)
        for index, eye in enumerate(walk.eyes):
            camera.location = eye
            eye_m = eye[2] + world.source.datum
            optical_depth.inputs[1].default_value = clear_air_depth(
                eye_m, scene.world.mist_settings.depth
            )
            filename = f"{index:04}.png"
            seconds = render_png(scene, pending / filename)
            frames.append(
                {"image": filename, "eye_enu_m": eye.tolist(), "seconds": seconds}
            )
        receipt = {
            **metadata,
            "distance_m": walk.route.length,
            "frame_spacing_m": walk.spacing_m,
            "forward_enu": world.look.tolist(),
            "route_enu_m": list(walk.route.coords),
            "support_probes": len(walk.foot_heights),
            "support_rise_m": float(walk.foot_heights[-1] - walk.foot_heights[0]),
            "maximum_probe_step_m": float(abs(np.diff(walk.foot_heights)).max()),
            "frames": frames,
        }
        (pending / "walk.json").write_text(json.dumps(receipt, indent=2) + "\n")
        for frame in frames:
            (pending / frame["image"]).replace(output / frame["image"])
        (pending / "walk.json").replace(output / "walk.json")
    print(
        json.dumps(
            {
                "walk": output.name,
                "frames": len(frames),
                "receipt": str(output / "walk.json"),
            }
        ),
        flush=True,
    )


def render(place, config, args):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    device = configure_gpu(scene, "eevee")
    configure_daylight(scene)
    world = ReferenceScene(place, config)
    walk_name = getattr(args, "walk", None)
    walk = plan_walk(world, config, config["walks"][walk_name]) if walk_name else None
    if walk and (world.clouds or world.aerosol_extinction or args.sky_dir):
        raise ValueError(f"{walk_name}: walk requires the clear procedural sky")
    if (
        walk
        and args.engine == "eevee"
        and any(
            reflection_planes(
                SimpleNamespace(
                    eye=eye,
                    look=world.look,
                    focal=world.focal,
                    water_plan=world.water_plan,
                    source=world.source,
                )
            )
            for eye in walk.eyes
        )
    ):
        raise ValueError(
            f"{walk_name}: movement across visible water needs per-view reflection capture"
        )
    sunlight = Vector(world.sun)
    sun = next(o for o in scene.objects if o.type == "LIGHT" and o.data.type == "SUN")
    sun.rotation_euler = (-sunlight).to_track_quat("-Z", "Y").to_euler()
    sun.data.energy *= world.direct_sun_fraction
    sun.data.shadow_maximum_resolution = SUN_SHADOW_MIN_TEXEL_M
    sun.data.use_shadow_jitter = True
    sky_dir = getattr(args, "sky_dir", None)
    sky_path = environment_path(sky_dir, place) if sky_dir is not None else None
    eye_m = world.eye[2] + world.source.datum
    if world.clouds and sky_path is None:
        raise ValueError(f"{place['id']}: export its cloud sky and specify --sky-dir")
    environment = (
        load_environment(
            sky_path,
            world.sun,
            eye_m,
            aerosol=world.aerosol_extinction,
            clouds=world.clouds,
        )
        if sky_path is not None
        else None
    )
    sun_energy = sun.data.energy
    cloud_lighting = None
    if environment is not None:
        if (
            args.engine == "eevee"
            and environment.cloud_sun_map is not None
            and world.sun[2] > 0
        ):
            cloud_lighting = CloudLighting(world, sun.data, environment.cloud_sun_map)
        sun.data.energy *= environment.cloud_sun
    sky_image = configure_world_sky(
        scene,
        sunlight,
        environment.radiance if environment else None,
        haze=bool(world.aerosol_extinction),
    )
    scene.cycles.seed = config["seed"]
    geometry = bpy.data.collections.new("World geometry")
    scene.collection.children.link(geometry)
    for name, verts, faces in world.surfaces():
        upload(name, verts, faces, collection=geometry)
    for name, verts, faces in world.grass_and_understory(config["seed"]):
        upload(name, verts, faces, collection=geometry)
    eye, look = Vector(walk.eyes[0] if walk else world.eye), Vector(world.look)
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
    woody_instances = place_woody_plants(
        world, config["seed"], geometry, eye_route=walk.route if walk else None
    )
    if world.water_bed is not None:
        apply_water_bed(world.water_bed)
    scene.eevee.taa_render_samples = 6
    receiver_start = perf_counter()
    bounce = (
        street_receivers(scene)
        if args.engine == "eevee"
        and getattr(args, "street_bounce", False)
        and sun.data.energy > 0
        else []
    )
    receiver_s = perf_counter() - receiver_start
    probes, bake_s = (
        bake_reference_irradiance(
            scene, world.local.covered_walks, world.eye[:2], street_sky=bool(bounce)
        )
        if args.engine == "eevee"
        else (0, 0.0)
    )
    bounce_count, bounce_s = bake_sun_bounce(scene, sun, bounce) if bounce else (0, 0.0)
    if cloud_lighting is not None:
        sun.data.energy = sun_energy
    planes = reflection_planes(world) if args.engine == "eevee" and walk is None else ()
    images = ()
    capture_s = 0.0
    if planes:
        images, capture_s = capture_images(
            scene, geometry, planes, cloud_lighting=cloud_lighting
        )
    if args.engine == "eevee" and (planes or sky_image is not None):
        for name in WATER_MATERIALS:
            if material := bpy.data.materials.get(name):
                apply_reflection(
                    material, world.eye, planes, images, sky_image=sky_image
                )
    surface_filter = None
    cloud_ambient_s = 0.0
    if cloud_lighting is not None:
        start = perf_counter()
        ambient = cloud_lighting.capture_ambient(scene)
        cloud_ambient_s = perf_counter() - start
        surface_filter = partial(cloud_lighting.mix, ambient=ambient)
    sky_s = configure_aerial(
        scene,
        geometry,
        world,
        eye_m,
        cache_sky=walk is None
        and args.repeat
        and (args.engine == "eevee" or sky_image is not None),
        surface_filter=surface_filter,
    )
    if args.engine == "cycles":
        device = configure_cycles_reference(
            scene,
            getattr(args, "cycles_samples", None) or CYCLES_REFERENCE_SAMPLES,
            args.repeat,
        )
    if walk:
        render_walk(
            scene,
            world,
            walk,
            args.output / walk_name,
            {
                "place": place["id"],
                "utc": place.get("utc", config["utc"]),
                "weather": world.weather,
                "seed": config["seed"],
                "source_snapshot": config["source_snapshot"],
                "engine": args.engine,
                "eevee_samples": scene.eevee.taa_render_samples
                if args.engine == "eevee"
                else None,
                "cycles_sample_cap": scene.cycles.samples
                if args.engine == "cycles"
                else None,
                "surface": world.support,
                "eye_offset_m": config["eye_height_m"],
                "origin_altitude_m": world.source.datum,
                "origin_wgs84": [world.source.lat, world.source.lon],
                "vertical_fov_deg": world.fov,
                "woody_instances": woody_instances,
                **device,
            },
        )
        return
    image = args.output / (place["id"] + ".png")
    args.output.mkdir(parents=True, exist_ok=True)
    times = [render_png(scene, image) for _ in range(2 if args.repeat else 1)]
    print(
        json.dumps(
            {
                "place": place["id"],
                "weather": world.weather,
                "cloud_sun": environment.cloud_sun if environment else 1.0,
                "spatial_cloud_sun": cloud_lighting is not None,
                "cloud_ambient_capture_s": round(cloud_ambient_s, 3),
                "sun_angle_rad": sun.data.angle,
                "eevee_sun_jitter": sun.data.use_shadow_jitter
                if args.engine == "eevee"
                else None,
                "eevee_samples": scene.eevee.taa_render_samples
                if args.engine == "eevee"
                else None,
                "cycles_sample_cap": scene.cycles.samples
                if args.engine == "cycles"
                else None,
                "surface": world.support,
                "woody_instances": woody_instances,
                "eye_m": eye_m,
                **device,
                "sky_radiance": str(sky_path) if sky_path is not None else None,
                "passage_irradiance_volumes": probes,
                "passage_irradiance_bake_s": round(bake_s, 3),
                "street_bounce": {
                    "samples": bounce_count,
                    "seconds": round(receiver_s + bounce_s, 3),
                }
                if bounce_count
                else None,
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
        "--walk",
        choices=config.get("walks", {}),
        help="render a mapped footpath sequence",
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
        "--cycles-samples",
        type=int,
        help=f"Cycles adaptive sample ceiling (default: {CYCLES_REFERENCE_SAMPLES})",
    )
    p.add_argument(
        "--sky-dir", type=Path, help="use exported scene-linear sky radiance"
    )
    p.add_argument("--repeat", action="store_true")
    p.add_argument(
        "--street-bounce",
        action="store_true",
        help="trace direct sunlight reflected onto nearby streets",
    )
    args = p.parse_args(
        sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    )
    if args.street_bounce and args.engine != "eevee":
        p.error("--street-bounce requires EEVEE")
    if args.cycles_samples is not None and (
        args.display_linear is not None
        or args.engine != "cycles"
        or args.cycles_samples < 1
    ):
        p.error("--cycles-samples requires a Cycles render and a positive integer")
    if args.display_linear is not None:
        display_linear(
            args.display_linear, args.output or ROOT / "build/shots/glsl-agx"
        )
    elif args.walk:
        args.output = args.output or ROOT / "build/shots/walks"
        (place,) = selected_views(config, [config["walks"][args.walk]["origin"]])
        render(place, config, args)
    else:
        args.output = args.output or ROOT / "build/shots/places"
        for place in selected_views(config, args.place):
            render(place, config, args)
