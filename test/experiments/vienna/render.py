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

from blender_air import configure_aerial
from blender_cloud import CloudLighting
from blender_reflection import apply_reflection, capture_images
from catalog import available_views, load_catalog, selected_views
from diffuse_bounce import (
    RECEIVER_REACH_M,
    bake_sun_bounce,
    reflection_index,
    street_receivers,
)
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
from passage_light import bake_reference_irradiance, near_lower_deck
from PIL import Image
from reference_scene import ReferenceScene
from regional_canopy import crown_batches
from shapely import Point
from sky_environment import environment_path, load_environment
from street_sky import bake_street_sky, capture_probe_diffuse, replace_probe_diffuse
from walk_reference import plan_walk
from water_bed import BED_ALBEDO
from water_reflection import reflection_planes

SUN_SHADOW_MIN_TEXEL_M = 0.016
CYCLES_REFERENCE_SAMPLES = 128
CYCLES_NOISE_THRESHOLD = 0.025


def detail_nodes(material, name, colour):
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
    fine_span, macro_span, _ = detail_scale(name)
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
    return (
        reflectance,
        channels.outputs["Green"],
        channels.outputs["Blue"],
        detail.outputs["Alpha"],
    )


def blend_trail_litter(material, colour, roughness, height):
    nodes = material.node_tree.nodes
    link = material.node_tree.links.new
    floor, floor_roughness, floor_height, litter = detail_nodes(
        material, "leaf litter", SURFACES["forest floor"][0]
    )
    mask = nodes.new("ShaderNodeAttribute")
    mask.attribute_name = "trail litter"
    edge = nodes.new("ShaderNodeMapRange")
    edge.interpolation_type = "SMOOTHSTEP"
    link(mask.outputs["Fac"], edge.inputs["Value"])
    exponent = nodes.new("ShaderNodeMath")
    exponent.operation = "MULTIPLY_ADD"
    link(litter, exponent.inputs[0])
    exponent.inputs[1].default_value = -0.70
    exponent.inputs[2].default_value = 1.35
    factor = nodes.new("ShaderNodeMath")
    factor.operation = "POWER"
    link(edge.outputs[0], factor.inputs[0])
    link(exponent.outputs[0], factor.inputs[1])
    mix = nodes.new("ShaderNodeMixRGB")
    link(factor.outputs[0], mix.inputs[0])
    link(colour, mix.inputs[1])
    link(floor, mix.inputs[2])
    properties = [mix.outputs[0]]
    for trail, shoulder in ((roughness, floor_roughness), (height, floor_height)):
        blend = nodes.new("ShaderNodeMix")
        blend.data_type = "FLOAT"
        link(factor.outputs[0], blend.inputs[0])
        link(trail, blend.inputs[2])
        link(shoulder, blend.inputs[3])
        properties.append(blend.outputs[0])
    return properties


def apply_detail(material, bsdf, name, colour):
    nodes = material.node_tree.nodes
    link = material.node_tree.links.new
    reflectance, roughness, height, _ = detail_nodes(material, name, colour)
    if name == "compacted":
        reflectance, roughness, height = blend_trail_litter(
            material, reflectance, roughness, height
        )
    link(reflectance, bsdf.inputs["Base Color"])
    link(roughness, bsdf.inputs["Roughness"])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Distance"].default_value = detail_scale(name)[2]
    link(height, bump.inputs["Height"])
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


def apply_distant_woodland(material, cover):
    with TemporaryDirectory(prefix="outshine-woodland-") as directory:
        path = Path(directory) / "Woodland coverage.png"
        Image.fromarray(cover.pixels[::-1]).save(path)
        image = bpy.data.images.load(str(path))
        image.colorspace_settings.name = "Non-Color"
        image.pack()

    tree = material.node_tree
    nodes, link = tree.nodes, tree.links.new
    point = nodes.new("ShaderNodeNewGeometry").outputs["Position"]
    coords = nodes.new("ShaderNodeVectorMath")
    coords.operation = "MULTIPLY_ADD"
    x, y, sx, sy = cover.transform
    coords.inputs[1].default_value = (sx, sy, 0)
    coords.inputs[2].default_value = (-x * sx, -y * sy, 0)
    link(point, coords.inputs[0])
    mask = nodes.new("ShaderNodeTexImage")
    mask.extension = "CLIP"
    mask.image = image
    link(coords.outputs[0], mask.inputs["Vector"])

    bsdf = nodes["Principled BSDF"]
    _, wood, wood_roughness = surface_properties("regional wood")
    colour = nodes.new("ShaderNodeMixRGB")
    colour.inputs[1].default_value = bsdf.inputs["Base Color"].default_value
    colour.inputs[2].default_value = (*wood, 1)
    link(mask.outputs["Color"], colour.inputs[0])
    link(colour.outputs[0], bsdf.inputs["Base Color"])
    roughness = nodes.new("ShaderNodeMath")
    roughness.operation = "MULTIPLY_ADD"
    base_roughness = bsdf.inputs["Roughness"].default_value
    roughness.inputs[1].default_value = wood_roughness - base_roughness
    roughness.inputs[2].default_value = base_roughness
    link(mask.outputs["Color"], roughness.inputs[0])
    link(roughness.outputs[0], bsdf.inputs["Roughness"])


def upload(name, vertices, faces, *, collection, smooth=False, trail_litter=None):
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
    if trail_litter is not None:
        data = (
            trail_litter[used]
            if isinstance(vertices, np.ndarray) and isinstance(faces, np.ndarray)
            else trail_litter
        )
        attribute = mesh.attributes.new("trail litter", "FLOAT", "POINT")
        attribute.data.foreach_set("value", data)
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


def place_regional_crowns(sites, geometry):
    for species, meshes, placements in crown_batches(sites):
        template = bpy.data.collections.new(species)
        for name, (vertices, faces) in meshes.items():
            upload(
                name,
                vertices,
                faces,
                collection=template,
                smooth=name.endswith(" crown"),
            )

        mesh = bpy.data.meshes.new(species)
        mesh.vertices.add(len(placements))
        mesh.vertices.foreach_set("co", placements[:, :3].ravel())
        for name, values in (("yaw", placements[:, 3]), ("size", placements[:, 4])):
            attribute = mesh.attributes.new(name, "FLOAT", "POINT")
            attribute.data.foreach_set("value", values.copy())
        mesh.update()
        obj = bpy.data.objects.new(species, mesh)
        geometry.objects.link(obj)

        group = bpy.data.node_groups.new(species, "GeometryNodeTree")
        for direction in ("INPUT", "OUTPUT"):
            group.interface.new_socket(
                name="Geometry", in_out=direction, socket_type="NodeSocketGeometry"
            )
        nodes, link = group.nodes, group.links.new
        info = nodes.new("GeometryNodeCollectionInfo")
        info.inputs["Collection"].default_value = template
        instances = nodes.new("GeometryNodeInstanceOnPoints")
        link(
            nodes.new("NodeGroupInput").outputs["Geometry"], instances.inputs["Points"]
        )
        link(info.outputs["Instances"], instances.inputs["Instance"])
        yaw = nodes.new("ShaderNodeCombineXYZ")
        for name, socket in (
            ("yaw", yaw.inputs["Z"]),
            ("size", instances.inputs["Scale"]),
        ):
            attribute = nodes.new("GeometryNodeInputNamedAttribute")
            attribute.data_type = "FLOAT"
            attribute.inputs["Name"].default_value = name
            link(attribute.outputs["Attribute"], socket)
        link(yaw.outputs["Vector"], instances.inputs["Rotation"])
        link(
            instances.outputs["Instances"],
            nodes.new("NodeGroupOutput").inputs["Geometry"],
        )
        obj.modifiers.new("Crown instances", "NODES").node_group = group


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
    observer_altitude = scene.compositing_node_group.nodes[
        "Observer altitude km"
    ].outputs[0]
    frames = []
    with TemporaryDirectory(prefix=".frames-", dir=output) as directory:
        pending = Path(directory)
        for index, eye in enumerate(walk.eyes):
            camera.location = eye
            eye_m = eye[2] + world.source.datum
            observer_altitude.default_value = eye_m * 0.001
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
    crown_sites = []
    for name, verts, faces in world.surfaces(crown_sites=crown_sites):
        upload(
            name,
            verts,
            faces,
            collection=geometry,
            trail_litter=world.trail_litter(name, verts),
        )
    if material := bpy.data.materials.get("distant terrain"):
        apply_distant_woodland(material, world.far_woodland)
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
    place_regional_crowns(crown_sites, geometry)
    if world.water_bed is not None:
        apply_water_bed(world.water_bed)
    scene.eevee.taa_render_samples = 6
    receiver_start = perf_counter()
    trace_sky = getattr(args, "street_sky", False)
    bounce = getattr(args, "street_bounce", False) and sun.data.energy > 0
    below_deck = (trace_sky or bounce) and near_lower_deck(
        world.viaducts, world.eye, RECEIVER_REACH_M
    )
    woodland_floor = world.woodland_floor if trace_sky and not below_deck else None
    woodland = woodland_floor is not None and woodland_floor.dwithin(
        Point(world.eye[:2]), 4.0
    )
    receivers = (
        street_receivers(scene, below_deck=below_deck, woodland=woodland)
        if args.engine == "eevee" and (trace_sky or bounce)
        else []
    )
    receiver_s = perf_counter() - receiver_start
    probes, bake_s = (
        bake_reference_irradiance(
            scene,
            world.local.covered_walks,
            world.eye[:2],
            street_sky=bool(receivers) and not (below_deck or woodland),
        )
        if args.engine == "eevee"
        else (0, 0.0)
    )
    ray_index = None
    ray_index_s = 0.0
    if trace_sky and receivers:
        start = perf_counter()
        graph = bpy.context.evaluated_depsgraph_get()
        ray_index = reflection_index(
            graph,
            np.asarray(scene.camera.location),
            world.sun if bounce else np.zeros(3),
            reflected_sky=below_deck or woodland,
        )
        ray_index_s = perf_counter() - start
    bounce_count, bounce_s = (
        bake_sun_bounce(scene, sun, receivers, index=ray_index)
        if bounce and receivers
        else (0, 0.0)
    )
    trace_count, trace_s = (
        bake_street_sky(
            scene,
            receivers,
            environment.radiance,
            index=ray_index,
            below_deck=below_deck,
            woodland=woodland,
        )
        if trace_sky and receivers
        else (0, 0.0)
    )
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
    diffuse_capture_s = 0.0
    if trace_count:
        start = perf_counter()
        captured = capture_probe_diffuse(scene, sun.data)
        diffuse_capture_s = perf_counter() - start
        surface_filter = partial(
            replace_probe_diffuse, captured=captured, surface_filter=surface_filter
        )
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
                "regional_crown_instances": sum(len(sites) for _, sites in crown_sites),
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
                "regional_crown_instances": sum(len(sites) for _, sites in crown_sites),
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
                "street_sky": {
                    "samples": trace_count,
                    "seconds": round(receiver_s + trace_s, 3),
                    "probe_diffuse_capture_s": round(diffuse_capture_s, 3),
                    "shared_index_s": round(ray_index_s, 3),
                    "shared_index_faces": len(ray_index.face_materials)
                    if ray_index is not None
                    else None,
                    "below_deck": below_deck,
                    "woodland": woodland,
                }
                if trace_count
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
    p.add_argument(
        "--street-sky",
        action="store_true",
        help="trace diffuse sky visibility for nearby street surfaces in EEVEE",
    )
    args = p.parse_args(
        sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    )
    if args.street_bounce and args.engine != "eevee":
        p.error("--street-bounce requires EEVEE")
    if args.street_sky and (
        args.engine != "eevee"
        or args.sky_dir is None
        or args.display_linear is not None
    ):
        p.error("--street-sky requires an EEVEE render and --sky-dir")
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
