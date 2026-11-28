"""Render selected Vienna reference views in a single Blender GPU process."""

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
from PIL import Image
from shapely import Point

from outshine.materials.ground_detail import (
    BARK_BOX_BLEND,
    DETAIL_TINTS,
    PLANAR_DETAILS,
    SURFACE_DETAIL,
    detail_scale,
    ground_detail,
)
from outshine.materials.palette import (
    DIFFUSE_TRANSMISSION,
    GLASS_IOR,
    GROUND_BLENDS,
    METALLIC,
    SURFACES,
    WATER_IOR,
    WATER_MATERIALS,
    surface_properties,
)
from outshine.rendering.blender_air import configure_aerial
from outshine.rendering.blender_camera import configure_camera
from outshine.rendering.blender_cloud import CloudLighting
from outshine.rendering.blender_light import (
    configure_daylight,
    configure_research_view,
)
from outshine.rendering.blender_reflection import apply_reflection, capture_images
from outshine.rendering.blender_render import configure_gpu, render_png
from outshine.rendering.diffuse_bounce import (
    RECEIVER_REACH_M,
    bake_sun_bounce,
    reflection_index,
    street_receivers,
)
from outshine.rendering.passage_light import (
    bake_light_probes,
    bake_reference_irradiance,
    near_lower_deck,
)
from outshine.rendering.sky_environment import environment_path, load_environment
from outshine.rendering.street_sky import (
    bake_street_sky,
    capture_probe_diffuse,
    replace_probe_diffuse,
)
from outshine.rendering.water_reflection import reflection_planes
from outshine.rendering.woody_visibility import (
    reference_visibility_test,
    visible_woody_placements,
)
from outshine.vegetation.landscape_cover import LANDSCAPE_MATERIALS
from outshine.vegetation.regional_canopy import crown_batches
from outshine.water.water_bed import BED_ALBEDO
from outshine.world.scene import ReferenceScene

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
    planar = name in PLANAR_DETAILS
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


def blend_ground(material, target, colour, roughness, height):
    nodes = material.node_tree.nodes
    link = material.node_tree.links.new
    ground, ground_roughness, ground_height, variation = detail_nodes(
        material, SURFACE_DETAIL[target], SURFACES[target][0]
    )
    mask = nodes.new("ShaderNodeAttribute")
    mask.attribute_name = "ground blend"
    edge = nodes.new("ShaderNodeMapRange")
    edge.interpolation_type = "SMOOTHSTEP"
    link(mask.outputs["Fac"], edge.inputs["Value"])
    exponent = nodes.new("ShaderNodeMath")
    exponent.operation = "MULTIPLY_ADD"
    link(variation, exponent.inputs[0])
    exponent.inputs[1].default_value = -0.70
    exponent.inputs[2].default_value = 1.35
    factor = nodes.new("ShaderNodeMath")
    factor.operation = "POWER"
    link(edge.outputs[0], factor.inputs[0])
    link(exponent.outputs[0], factor.inputs[1])
    mix = nodes.new("ShaderNodeMixRGB")
    link(factor.outputs[0], mix.inputs[0])
    link(colour, mix.inputs[1])
    link(ground, mix.inputs[2])
    properties = [mix.outputs[0]]
    for inner, outer in ((roughness, ground_roughness), (height, ground_height)):
        blend = nodes.new("ShaderNodeMix")
        blend.data_type = "FLOAT"
        link(factor.outputs[0], blend.inputs[0])
        link(inner, blend.inputs[2])
        link(outer, blend.inputs[3])
        properties.append(blend.outputs[0])
    return properties


def apply_detail(material, bsdf, name, colour, ground_target):
    nodes = material.node_tree.nodes
    link = material.node_tree.links.new
    reflectance, roughness, height, _ = detail_nodes(material, name, colour)
    if ground_target is not None:
        reflectance, roughness, height = blend_ground(
            material, ground_target, reflectance, roughness, height
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
        apply_detail(material, bsdf, detail, colour, GROUND_BLENDS.get(base))
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


def cover_image(name, cover):
    with TemporaryDirectory(prefix="outshine-land-cover-") as directory:
        path = Path(directory) / f"{name}.png"
        Image.fromarray(cover.pixels[::-1]).save(path)
        image = bpy.data.images.load(str(path))
        image.colorspace_settings.name = "Non-Color"
        image.alpha_mode = "CHANNEL_PACKED"
        image.pack()
    return image


def cover_texture(material, transform, image):
    tree = material.node_tree
    nodes, link = tree.nodes, tree.links.new
    point = nodes.new("ShaderNodeNewGeometry").outputs["Position"]
    coords = nodes.new("ShaderNodeVectorMath")
    coords.operation = "MULTIPLY_ADD"
    x, y, sx, sy = transform
    coords.inputs[1].default_value = (sx, sy, 0)
    coords.inputs[2].default_value = (-x * sx, -y * sy, 0)
    link(point, coords.inputs[0])
    texture = nodes.new("ShaderNodeTexImage")
    texture.extension = "EXTEND"
    texture.image = image
    link(coords.outputs[0], texture.inputs["Vector"])
    return texture.outputs


def apply_distant_cover(material, cover):
    texture = cover_texture(
        material, cover.transform, cover_image("Horizon material", cover)
    )
    tree = material.node_tree
    bsdf = tree.nodes["Principled BSDF"]
    tree.links.new(texture["Color"], bsdf.inputs["Base Color"])
    tree.links.new(texture["Alpha"], bsdf.inputs["Roughness"])


def apply_landscape_cover(cover):
    image = cover_image("Landscape vegetation", cover)
    for name in sorted(LANDSCAPE_MATERIALS):
        material = bpy.data.materials.get(name)
        if material is None:
            continue
        texture = cover_texture(material, cover.transform, image)
        tree = material.node_tree
        base = tree.nodes["Principled BSDF"].inputs["Base Color"]
        weight = tree.nodes.new("ShaderNodeMath")
        weight.operation = "SUBTRACT"
        weight.inputs[0].default_value = 1
        tree.links.new(texture["Alpha"], weight.inputs[1])
        mix = tree.nodes.new("ShaderNodeVectorMath")
        mix.operation = "MULTIPLY_ADD"
        if base.is_linked:
            tree.links.new(base.links[0].from_socket, mix.inputs[0])
        else:
            mix.inputs[0].default_value = base.default_value[:3]
        tree.links.new(weight.outputs[0], mix.inputs[1])
        tree.links.new(texture["Color"], mix.inputs[2])
        tree.links.new(mix.outputs["Vector"], base)


def upload(name, vertices, faces, *, collection, smooth=False, ground_blend=None):
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
    if ground_blend is not None:
        data = (
            ground_blend[used]
            if isinstance(vertices, np.ndarray) and isinstance(faces, np.ndarray)
            else ground_blend
        )
        attribute = mesh.attributes.new("ground blend", "FLOAT", "POINT")
        attribute.data.foreach_set("value", data)
    if smooth:
        mesh.polygons.foreach_set("use_smooth", np.ones(len(mesh.polygons), dtype=bool))
    mesh.update()
    mesh.materials.append(surface_material(name))
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    return obj


def place_woody_plants(world, seed, geometry, *, eye_route=None, may_contribute=None):
    count = 0
    for species, group, meshes, placements in world.woody_plants(
        seed, eye_route=eye_route
    ):
        if group is None:
            for name, (vertices, faces) in meshes.items():
                upload(name, vertices, faces, collection=geometry, smooth=True)
            continue
        if may_contribute is not None:
            placements = visible_woody_placements(meshes, placements, may_contribute)
            if not len(placements):
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


def configure_cycles_reference(scene, samples):
    device = configure_gpu(scene, "cycles")
    scene.cycles.samples = samples
    scene.cycles.use_adaptive_sampling = True
    scene.cycles.adaptive_threshold = CYCLES_NOISE_THRESHOLD
    scene.cycles.adaptive_min_samples = min(8, samples)
    scene.cycles.use_denoising = True
    scene.cycles.denoising_use_gpu = True
    return device


def render(place, config, args):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    device = configure_gpu(scene, "eevee")
    configure_daylight(scene)
    world = ReferenceScene(place, config)
    sunlight = Vector(world.sun)
    sun = next(o for o in scene.objects if o.type == "LIGHT" and o.data.type == "SUN")
    sun.rotation_euler = (-sunlight).to_track_quat("-Z", "Y").to_euler()
    transmission = world.direct_sun_transmission
    peak = max(transmission)
    sun.data.energy *= peak
    if peak > 0:
        sun.data.color = tuple(channel / peak for channel in transmission)
    sun.data.shadow_maximum_resolution = SUN_SHADOW_MIN_TEXEL_M
    sun.data.use_shadow_jitter = True
    sky_dir = Path(args.sky_dir) if args.sky_dir is not None else None
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
            ground_blend=world.ground_blend(name, verts),
        )
    if material := bpy.data.materials.get("distant terrain"):
        apply_distant_cover(material, world.terrain_cover)
    if cover := world.landscape_cover:
        apply_landscape_cover(cover)
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
    trace_sky = getattr(args, "street_sky", False)
    bounce = getattr(args, "street_bounce", False) and sun.data.energy > 0
    planes = reflection_planes(world) if args.engine == "eevee" else ()
    may_contribute = None
    # Probe, bounce and cloud captures can sample geometry outside the camera views.
    if (
        args.engine == "eevee"
        and sun.data.energy > 0
        and not (trace_sky or bounce or world.clouds or world.local.covered_walks)
    ):
        may_contribute = reference_visibility_test(
            world,
            planes,
            scene.render.resolution_x,
            scene.render.resolution_y,
            sun.data.angle,
        )
    woody_instances = place_woody_plants(
        world, config["seed"], geometry, may_contribute=may_contribute
    )
    place_regional_crowns(crown_sites, geometry)
    if world.water_bed is not None:
        apply_water_bed(world.water_bed)
    scene.eevee.taa_render_samples = 6
    receiver_start = perf_counter()
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
            sun.data,
            street_sky=bool(receivers) and not (below_deck or woodland),
            woodland=woodland,
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
    woodland_bounce_s = 0.0
    if (
        woodland
        and args.engine == "eevee"
        and sun.data.energy > 0
        and not bounce
        and cloud_lighting is None
    ):
        # The subtraction image contains only skylight, so reflected sun remains.
        woodland_bounce_s = bake_light_probes()
    sky_s = configure_aerial(
        scene,
        geometry,
        world,
        eye_m,
        surface_filter=surface_filter,
    )
    if args.engine == "cycles":
        device = configure_cycles_reference(
            scene,
            getattr(args, "cycles_samples", None) or CYCLES_REFERENCE_SAMPLES,
        )
    image = Path(args.output)
    image.parent.mkdir(parents=True, exist_ok=True)
    seconds = render_png(scene, image)
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
                "woodland_sky_volume": args.engine == "eevee" and woodland,
                "woodland_sun_bake_s": round(woodland_bounce_s, 3),
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
                "seconds_to_png": seconds,
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


def render_request():
    place, config, options = json.loads(
        Path(sys.argv[sys.argv.index("--") + 1]).read_text()
    )
    render(place, config, SimpleNamespace(**options))


def display_request():
    paths, output = json.loads(Path(sys.argv[sys.argv.index("--") + 1]).read_text())
    display_linear(tuple(map(Path, paths)), Path(output))
