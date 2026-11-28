"""Render generated reference geometry with the shared Outshine GLSL on the hardware GPU."""

import json
import re
from dataclasses import dataclass
from itertools import chain
from pathlib import Path
from time import perf_counter

import moderngl
import numpy as np
from PIL import Image

from outshine.materials.ground_detail import (
    BARK_BOX_BLEND,
    DETAIL_TINTS,
    LITTER_DETAILS,
    PLANAR_DETAILS,
    SURFACE_DETAIL,
    detail_scale,
    ground_detail,
)
from outshine.materials.palette import (
    DIFFUSE_TRANSMISSION,
    GLASS_IOR,
    METALLIC,
    SURFACES,
    WATER_IOR,
    WATER_MATERIALS,
    surface_properties,
)
from outshine.rendering.sky_environment import (
    CLOUD_ENVIRONMENT_SIZE,
    SKY_ENVIRONMENT_SIZE,
    diffuse_coefficients,
    environment_path,
    load_environment,
    save_environment,
)
from outshine.rendering.water_reflection import (
    LEVEL_TOLERANCE_M,
    REFLECTION_RANGE_M,
    camera_axes,
    reflection_planes,
)
from outshine.vegetation.regional_canopy import crown_batches
from outshine.water.water_bed import BED_ALBEDO
from outshine.weather.air_profile import EARTH_RADIUS_KM
from outshine.weather.cloud_profile import SUN_MAP_RADIUS_KM, SUN_MAP_SIZE, cloud_noise
from outshine.world.scene import ReferenceScene

HERE = Path(__file__).with_name("shaders")
ROOT = Path(__file__).resolve().parents[3]

REFLECTION_SIZE = (640, 360)
REGIONAL_SHADOW_SPAN = (4800.0, 18000.0)
LOCAL_SHADOW_SPAN = (96.0, 1024.0)
NEAR_SHADOW_WIDTH = 16.0
NEAR_SHADOW_RESOLUTION = 2048

SCREEN = """#version 410 core
out vec2 ndc;
void main(){ndc=vec2((gl_VertexID<<1)&2,gl_VertexID&2)*2.-1.;gl_Position=vec4(ndc,0.,1.);}
"""


def shader(path):
    src = path.read_text()
    if path.name == "display.glsl":
        src = src.replace(
            "layout(std140, set = 3, binding = DISPLAY_BINDING)", "layout(std140)"
        )
    # GLSL 4.10 requires constant initializers for local const declarations.
    if path.name == "AtmosphereCore.h":
        src = re.sub(r"(?m)^(\s+)const (float|bool) ", r"\1\2 ", src)

    def include(m):
        name = m[1]
        target = next(
            p
            for p in (
                HERE / name,
                ROOT / "src/render/shaders" / name,
                ROOT / "src" / name,
            )
            if p.is_file()
        )
        return shader(target)

    return re.sub(r'^#include "([^"]+)"', include, src, flags=re.MULTILINE)


def atmosphere(
    ctx,
    target,
    world,
    right,
    up,
    *,
    environment=False,
    linear=False,
    archive=None,
    lighting_target=None,
):
    prog = ctx.program(
        vertex_shader=SCREEN, fragment_shader=shader(HERE / "atmosphere.frag")
    )
    quad = ctx.vertex_array(prog, [])
    prog["linearOutput"].value = linear
    for key, value in {
        "eyeKm": EARTH_RADIUS_KM + (world.eye[2] + world.source.datum) * 0.001,
        "aerosolExtinction": world.aerosol_extinction,
        "sun": tuple(world.sun),
        "rightRay": tuple(right),
        "upRay": tuple(up),
        "forward": tuple(world.look),
    }.items():
        prog[key].value = value
    placeholder = ctx.texture(
        (1, 1), 4, dtype="f4", data=np.zeros(4, dtype="f4").tobytes()
    )
    for unit, key in enumerate(("transmittance", "multiScatter", "skyView")):
        placeholder.use(unit)
        prog[key].value = unit
    clouds = world.clouds
    prog["cloudKind"].value = clouds.kind if clouds else 0
    prog["skyArchive"].value = 4
    (archive or placeholder).use(4)
    volume = (
        cloud_noise(clouds.seed)
        if environment and clouds
        else np.zeros((1, 1, 1, 3), dtype="f4")
    )
    shape_texture = ctx.texture3d(volume.shape[2::-1], 3, volume.tobytes(), dtype="f4")
    shape_texture.filter = (moderngl.LINEAR, moderngl.LINEAR)
    shape_texture.use(3)
    prog["cloudNoise"].value = 3
    if clouds and environment:
        prog["cloudLayer"].value = clouds.layer
        prog["cloudOriginKm"].value = clouds.origin_km
    textures = []
    for i, (key, size) in enumerate(
        (
            ("transmittance", (512, 64)),
            ("multiScatter", (32, 32)),
            ("skyView", (192, 108)),
        )
    ):
        tex = ctx.texture(size, 4, dtype="f4")
        tex.repeat_x = tex.repeat_y = False
        dest = ctx.framebuffer([tex])
        dest.use()
        prog["stage"].value = i
        prog["extent"].value = size
        quad.render(vertices=3)
        dest.release()
        textures.append(tex)
        tex.use(i)
        prog[key].value = i
    target.use()
    prog["stage"].value = 4 if environment else 3
    prog["extent"].value = target.size
    quad.render(vertices=3)
    if lighting_target is not None:
        lighting_target.use()
        prog["stage"].value = 4
        prog["extent"].value = lighting_target.size
        quad.render(vertices=3)
    cloud_sun, sun_map = 1.0, None
    if environment and clouds:
        probe = ctx.simple_framebuffer((1, 1), components=4, dtype="f4")
        probe.use()
        prog["stage"].value = 5
        quad.render(vertices=3)
        cloud_sun = float(
            np.frombuffer(probe.read(components=1, dtype="f4"), dtype="f4")[0]
        )
        probe.release()
        sun_map_target = ctx.simple_framebuffer(
            (SUN_MAP_SIZE, SUN_MAP_SIZE), 1, dtype="f4"
        )
        sun_map_target.use()
        prog["stage"].value = 6
        prog["extent"].value = sun_map_target.size
        prog["cloudSunMapRadiusKm"].value = SUN_MAP_RADIUS_KM
        origin_m = (
            clouds.sun_map_origin(world.eye, world.source.datum)[2] + world.source.datum
        )
        prog["cloudSunMapOriginKm"].value = EARTH_RADIUS_KM + origin_m * 0.001
        prog["cloudObserverSun"].value = cloud_sun
        quad.render(vertices=3)
        sun_map = np.frombuffer(
            sun_map_target.read(components=1, dtype="f4"), dtype="f4"
        ).reshape(SUN_MAP_SIZE, SUN_MAP_SIZE)
        sun_map_target.release()
    shape_texture.release()
    placeholder.release()
    quad.release()
    prog.release()
    veil, multi_scatter, sky_view = textures
    multi_scatter.release()
    return veil, sky_view, cloud_sun, sun_map


def export_sky(ctx, place, config, out):
    world = ReferenceScene(place, config)
    _, right, up = projection(world)
    size = CLOUD_ENVIRONMENT_SIZE if world.clouds else SKY_ENVIRONMENT_SIZE
    target = ctx.simple_framebuffer(size, components=4, dtype="f4")
    ctx.disable(moderngl.DEPTH_TEST)
    veil, sky, cloud_sun, sun_map = atmosphere(
        ctx, target, world, right, up, environment=True
    )
    width, height = target.size
    rgba = np.frombuffer(target.read(components=4, dtype="f4"), dtype="f4").reshape(
        height, width, 4
    )
    opacity = rgba[height // 2 :, :, 3]
    elevation = (np.arange(len(opacity)) + 0.5) * (0.5 * np.pi / len(opacity))
    cloud_cover = float(
        np.average((opacity >= 0.5).mean(axis=1), weights=np.cos(elevation))
    )
    path = environment_path(out, place)
    eye_m = world.eye[2] + world.source.datum
    save_environment(
        path,
        rgba[..., :3].copy(),
        world.sun,
        eye_m,
        aerosol=world.aerosol_extinction,
        clouds=world.clouds,
        cloud_sun=cloud_sun,
        cloud_sun_map=sun_map,
    )
    for resource in (veil, sky, target):
        resource.release()
    print(
        json.dumps(
            {
                "place": place["id"],
                "weather": world.weather,
                "sky": str(path),
                "cloud_sun": cloud_sun,
                "cloud_cover": cloud_cover,
                "cloud_recipe_version": world.clouds.archive_key[0]
                if world.clouds
                else None,
                "eye_m": eye_m,
            }
        ),
        flush=True,
    )


def sky_radiance_texture(ctx, radiance):
    source = ctx.texture(radiance.shape[1::-1], 3, radiance.tobytes(), dtype="f4")
    source.repeat_x = True
    source.repeat_y = False
    source.filter = (moderngl.LINEAR, moderngl.LINEAR)
    return source


def sky_specular_atlas(ctx, source):
    native_layout = ROOT / "src/render/stages/EnvironmentSpecularValues.inc"
    values = dict(re.findall(r"ENV_INT\((\w+), (\d+)\)", native_layout.read_text()))
    span = int(values["kEnvironmentSide"]) + 2 * int(values["kEnvironmentBorder"])
    source.use(0)
    prog = ctx.program(
        vertex_shader=SCREEN, fragment_shader=shader(HERE / "sky_specular.frag")
    )
    prog["skyRadiance"].value = 0
    quad = ctx.vertex_array(prog, [])
    atlas = ctx.texture((span, span * int(values["kEnvironmentLevels"])), 4, dtype="f2")
    brdf = ctx.texture((64, 64), 2, dtype="f2")
    for integrate_brdf, texture in ((False, atlas), (True, brdf)):
        texture.repeat_x = texture.repeat_y = False
        texture.filter = (moderngl.LINEAR, moderngl.LINEAR)
        target = ctx.framebuffer([texture])
        target.use()
        target.clear()
        prog["integrateBrdf"].value = integrate_brdf
        prog["extent"].value = texture.size
        quad.render(vertices=3)
        target.release()
    for resource in (quad, prog):
        resource.release()
    return atlas, brdf


def projection(world):
    forward = world.look
    right, up = camera_axes(forward)
    view = np.eye(4)
    view[:3, :3] = np.array([right, up, -forward])
    view[:3, 3] = -view[:3, :3] @ world.eye
    t = 360 / world.focal
    near = 0.05
    far = 240000.0
    clip = np.array(
        [
            [720 / (1280 * t), 0, 0, 0],
            [0, 1 / t, 0, 0],
            [0, 0, -(far + near) / (far - near), -2 * far * near / (far - near)],
            [0, 0, -1, 0],
        ]
    )
    return clip @ view, right * t * 1280 / 720, up * t


@dataclass(frozen=True, slots=True)
class DrawBatch:
    name: str
    vao: moderngl.VertexArray
    points: moderngl.Buffer
    indices: moderngl.Buffer
    placements: np.ndarray
    bounds: tuple[np.ndarray, np.ndarray] | None
    instance_and_vertex_buffers: tuple[moderngl.Buffer, ...]

    def release(self):
        self.vao.release()
        self.points.release()
        self.indices.release()
        for buffer in self.instance_and_vertex_buffers:
            buffer.release()


def world_caster_bounds(vertices, placements):
    coordinates = vertices.T
    lower = np.array([axis.min() for axis in coordinates])
    upper = np.array([axis.max() for axis in coordinates])
    centre, half_size = (lower + upper) * 0.5, (upper - lower) * 0.5
    position, angle, scale = placements[:, :3], placements[:, 3], placements[:, 4]
    cosine, sine = np.cos(angle), np.sin(angle)
    axes = np.zeros((len(placements), 3, 3), dtype="f4")
    axes[:, 0, 0], axes[:, 0, 1] = cosine, sine
    axes[:, 1, 0], axes[:, 1, 1] = -sine, cosine
    axes[:, 2, 2] = 1
    centre = position + scale[:, None] * (centre @ axes)
    half_axes = axes * (scale[:, None] * half_size)[:, :, None]
    return centre, half_axes


def shadow_caster_mask(projection, centres, half_axes):
    """Project oriented bounds into the orthographic shadow clip volume."""
    transform = projection[:3, :3].T
    centre = centres @ transform + projection[:3, 3]
    half_size = np.abs(half_axes @ transform).sum(axis=1)
    # Allow for float32 differences between CPU bounds and the vertex shader.
    return (np.abs(centre) <= 1.0001 + half_size).all(axis=1)


def upload(ctx, program, name, vertices, faces, placements, *, ground_litter=None):
    faces = np.asarray(faces, dtype="i4")
    if not faces.size:
        return
    if faces.shape[1] == 4:
        faces = faces[:, [0, 1, 2, 0, 2, 3]]
    vertices = np.asarray(vertices, dtype="f4")
    placements = np.asarray(placements, dtype="f4")
    points = ctx.buffer(vertices.tobytes())
    idx = ctx.buffer(faces.tobytes())
    instances = ctx.buffer(placements.tobytes())
    content = [
        (points, "3f", "position"),
        (instances, "4f 1f /i", "offsetAngle", "scale"),
    ]
    buffers = [instances]
    if ground_litter is not None:
        litter = ctx.buffer(ground_litter.tobytes())
        content.append((litter, "1f", "groundLitter"))
        buffers.append(litter)
    if name.endswith((" bark", " crown")):
        a, b, c = vertices[faces].transpose(1, 0, 2)
        area_normals = np.cross(b - a, c - a)
        normals = np.zeros_like(vertices)
        for corner in faces.T:
            np.add.at(normals, corner, area_normals)
        lengths = np.linalg.norm(normals, axis=1, keepdims=True)
        normals /= np.maximum(lengths, np.finfo(normals.dtype).tiny)
        shaded = ctx.buffer(normals.tobytes())
        content.append((shaded, "3f", "vertexNormal"))
        buffers.append(shaded)
    vao = ctx.vertex_array(program, content, idx)
    bounds = world_caster_bounds(vertices, placements) if len(placements) > 1 else None
    return DrawBatch(
        name,
        vao,
        points,
        idx,
        placements,
        bounds,
        tuple(buffers),
    )


def shadow_projection(world, span, forward_offset, *, resolution=None):
    right = np.cross([0.0, 0.0, 1.0], world.sun)
    if np.linalg.norm(right) < 1e-6:
        right = np.array([1.0, 0.0, 0.0])
    right /= np.linalg.norm(right)
    up = np.cross(world.sun, right)
    center = world.eye + forward_offset * world.look
    width, depth = span
    light = np.eye(4)
    light[:3, :3] = np.array(
        [2 * right / width, 2 * up / width, -2 * world.sun / depth]
    )
    light[:3, 3] = -light[:3, :3] @ center
    if resolution is not None:
        step = 2.0 / resolution
        light[:2, 3] = np.rint(light[:2, 3] / step) * step
    return light


def shadow_casters(ctx, prog, draws, projection):
    casters, filtered = [], []
    for batch in draws:
        if batch.name in WATER_MATERIALS:
            continue
        if batch.bounds is None:
            casters.append((len(batch.placements), batch.vao))
            continue
        keep = shadow_caster_mask(projection, *batch.bounds)
        count = np.count_nonzero(keep)
        if not count:
            continue
        vao = batch.vao
        if count != len(batch.placements):
            instances = ctx.buffer(batch.placements[keep].tobytes())
            vao = ctx.vertex_array(
                prog,
                [
                    (batch.points, "3f", "position"),
                    (instances, "4f 1f /i", "offsetAngle", "scale"),
                ],
                batch.indices,
            )
            filtered.extend((vao, instances))
        casters.append((count, vao))
    return casters, filtered


def shadow_depth(ctx, prog, casters, light, size):
    texture = ctx.depth_texture((size, size))
    texture.repeat_x = texture.repeat_y = False
    texture.filter = (moderngl.LINEAR, moderngl.LINEAR)
    target = ctx.framebuffer(depth_attachment=texture)
    target.use()
    target.clear(depth=1)
    prog["viewProjection"].write(light)
    for count, vao in casters:
        vao.render(instances=count)
    target.release()
    return texture


def render_shadows(ctx, prog, draws, world):
    regional = shadow_projection(world, REGIONAL_SHADOW_SPAN, 1000)
    local = shadow_projection(world, LOCAL_SHADOW_SPAN, 12)
    near = shadow_projection(
        world,
        (NEAR_SHADOW_WIDTH, LOCAL_SHADOW_SPAN[1]),
        3,
        resolution=NEAR_SHADOW_RESOLUTION,
    )
    # The two maps share the sunward plane used to test regional casters.
    near[2] = local[2]
    passes = (
        ("shadowMap", "shadowMatrix", 3, regional, 4096),
        ("localShadowMap", "localShadowMatrix", 6, local, 2048),
        ("nearShadowMap", "nearShadowMatrix", 9, near, NEAR_SHADOW_RESOLUTION),
    )
    prog["regionalShadowSpan"].value = REGIONAL_SHADOW_SPAN
    prog["localShadowSpan"].value = LOCAL_SHADOW_SPAN
    prog["nearShadowWidth"].value = NEAR_SHADOW_WIDTH
    placeholder = ctx.depth_texture((1, 1))
    for sampler, matrix, unit, projection, _ in passes:
        placeholder.use(unit)
        prog[sampler].value = unit
        prog[matrix].write(projection.T.astype("f4").tobytes())
    blocker_units = (("localDepthMap", 14), ("nearDepthMap", 15))
    for name, unit in blocker_units:
        placeholder.use(unit)
        prog[name].value = unit
    caster_lists = [
        shadow_casters(ctx, prog, draws, matrix) for _, _, _, matrix, _ in passes
    ]
    prog["shadowPass"].value = True
    query = ctx.query(time=True)
    with query:
        textures = [
            shadow_depth(ctx, prog, casters, matrix.T.astype("f4").tobytes(), size)
            for (_, _, _, matrix, size), (casters, _) in zip(
                passes, caster_lists, strict=True
            )
        ]
    prog["shadowPass"].value = False
    for _, resources in caster_lists:
        for resource in resources:
            resource.release()
    for (_, _, unit, _, _), texture in zip(passes, textures, strict=True):
        texture.use(unit)
    depth_samplers = []
    for (_, unit), texture in zip(blocker_units, textures[1:], strict=True):
        sampler = ctx.sampler(
            texture=texture,
            repeat_x=False,
            repeat_y=False,
            filter=(moderngl.NEAREST, moderngl.NEAREST),
        )
        sampler.use(unit)
        depth_samplers.append(sampler)
    placeholder.release()
    return (*textures, *depth_samplers), query


def detail_texture(ctx, textures, detail):
    if detail not in textures:
        pixels = ground_detail(detail)
        texture = ctx.texture(pixels.shape[:2], 4, pixels.tobytes(), dtype="f4")
        texture.build_mipmaps()
        textures[detail] = texture
    return textures[detail]


def draw_surfaces(ctx, prog, draws, textures, *, capture=False):
    for batch in draws:
        name = batch.name
        water = name in WATER_MATERIALS
        if capture and water:
            continue
        base, rgb, rough = surface_properties(name)
        prog["surface"].value = (*rgb, rough)
        prog["metalness"].value = METALLIC.get(name, 0.0)
        prog["diffuseTransmission"].value = DIFFUSE_TRANSMISSION.get(name, 0.0)
        prog["waterSurface"].value = water
        prog["hasTerrainCover"].value = base == "distant terrain"
        prog["glassIor"].value = GLASS_IOR if name == "glass" else 1.0
        detail = SURFACE_DETAIL.get(base)
        litter = detail in LITTER_DETAILS
        prog["hasGroundLitter"].value = litter
        prog["hasDetail"].value = detail is not None
        prog["blendDetailAxes"].value = base.endswith(" bark")
        prog["planarDetail"].value = detail in PLANAR_DETAILS
        prog["detailTint"].value = DETAIL_TINTS.get(detail, (1, 1, 1))
        if detail is not None:
            prog["detailScale"].value = detail_scale(detail)
            detail_texture(ctx, textures, detail).use(2)
        if litter:
            detail_texture(ctx, textures, "leaf litter").use(12)
        batch.vao.render(instances=len(batch.placements))


def capture_reflections(ctx, prog, draws, textures, world, projection_matrix, planes):
    prog["reflectionMap"].value = 4
    prog["reflectionRange"].value = REFLECTION_RANGE_M
    prog["reflectionTolerance"].value = LEVEL_TOLERANCE_M
    for i in range(2):
        coefficients = planes[i].coefficients() if i < len(planes) else (0.0,) * 4
        prog[f"reflectionPlane{i}"].value = coefficients
    prog["reflectionCount"].value = len(planes)
    placeholder = ctx.texture((1, 1), 4, data=b"\0" * 4)
    placeholder.use(4)
    if not planes:
        return placeholder, 0.0
    width, height = REFLECTION_SIZE
    size = (width, height * len(planes))
    atlas = ctx.texture(size, 4, dtype="f2")
    atlas.repeat_x = atlas.repeat_y = False
    atlas.filter = (moderngl.LINEAR, moderngl.LINEAR)
    depth = ctx.depth_renderbuffer(size)
    capture = ctx.framebuffer([atlas], depth)
    capture.use()
    capture.clear(0, 0, 0, 0, depth=1)
    prog["reflectionCapture"].value = True
    query = ctx.query(time=True)
    with query:
        for slot, plane in enumerate(planes):
            ctx.viewport = (0, slot * height, width, height)
            mirror = plane.mirror()
            prog["viewProjection"].write(
                (projection_matrix @ mirror).T.astype("f4").tobytes()
            )
            prog["eye"].value = tuple((mirror @ np.r_[world.eye, 1.0])[:3])
            prog["reflectionClip"].value = (
                *plane.normal,
                plane.offset + LEVEL_TOLERANCE_M / 2,
            )
            draw_surfaces(ctx, prog, draws, textures, capture=True)
    prog["reflectionCapture"].value = False
    prog["eye"].value = tuple(world.eye)
    capture.release()
    depth.release()
    placeholder.release()
    atlas.use(4)
    return atlas, query.elapsed * 1e-6


def save_view(target, path, *, linear):
    path.parent.mkdir(parents=True, exist_ok=True)
    if linear:
        width, height = target.size
        data = target.read(components=3, dtype="f4", alignment=1)
        rgb = np.frombuffer(data, dtype="f4").reshape(height // 2, 2, width // 2, 2, 3)
        np.save(path.with_suffix(".npy"), rgb.mean(axis=(1, 3))[::-1])
        return
    image = Image.frombytes(
        "RGB", target.size, target.read(components=3, alignment=1)
    ).transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    image.resize((1280, 720), Image.Resampling.BOX).save(path)


def render(ctx, place, config, output, *, sky_dir=None, linear=False):
    start = perf_counter()
    world = ReferenceScene(place, config)
    sky_path = environment_path(sky_dir, place) if sky_dir is not None else None
    if world.clouds and sky_path is None:
        raise ValueError(f"{place['id']}: export its cloud sky and specify --sky-dir")
    environment = (
        load_environment(
            sky_path,
            world.sun,
            world.eye[2] + world.source.datum,
            aerosol=world.aerosol_extinction,
            clouds=world.clouds,
        )
        if sky_path is not None
        else None
    )
    source = sky_radiance_texture(ctx, environment.radiance) if environment else None
    target = ctx.simple_framebuffer(
        (2560, 1440), components=4, dtype="f4" if linear else "f1"
    )
    target.use()
    target.clear(depth=1)
    ctx.disable(moderngl.DEPTH_TEST)
    m, right, up = projection(world)
    lighting_target = (
        ctx.simple_framebuffer(SKY_ENVIRONMENT_SIZE, components=3, dtype="f4")
        if environment is None
        else None
    )
    veil, sky, _, _ = atmosphere(
        ctx,
        target,
        world,
        right,
        up,
        linear=linear,
        archive=source,
        lighting_target=lighting_target,
    )
    if lighting_target is None:
        radiance = environment.radiance
    else:
        radiance = np.frombuffer(
            lighting_target.read(components=3, dtype="f4"), dtype="f4"
        ).reshape(*SKY_ENVIRONMENT_SIZE[::-1], 3)
        lighting_target.release()
        source = sky_radiance_texture(ctx, radiance)
    prog = ctx.program(
        vertex_shader=shader(HERE / "reference.vert"),
        fragment_shader=shader(HERE / "reference.frag"),
    )
    prog["viewProjection"].write(m.T.astype("f4").tobytes())
    prog["eye"].value = tuple(world.eye)
    prog["sun"].value = tuple(world.sun)
    cloud_sun = environment.cloud_sun if environment else 1.0
    cloud_map = environment.cloud_sun_map if environment and world.sun[2] > 0 else None
    direct_sun = (
        2.5
        * np.asarray(world.direct_sun_transmission)
        * (cloud_sun if cloud_map is None else 1.0)
    )
    prog["directSunRadiance"].value = tuple(direct_sun)
    prog["cloudSunMapScale"].value = (
        0 if cloud_map is None else 1e-3 / (2 * SUN_MAP_RADIUS_KM)
    )
    prog["cloudSunMapTopM"].value = (
        world.clouds.layer[1] * 1000 - world.source.datum
        if cloud_map is not None
        else 0
    )
    prog["cloudSunMapOrigin"].value = (
        tuple(world.clouds.sun_map_origin(world.eye, world.source.datum))
        if cloud_map is not None
        else (0, 0, 0)
    )
    prog["cloudSunMap"].value = 11
    sun_map_texture = None
    if cloud_map is not None:
        sun_map_texture = ctx.texture(
            cloud_map.shape[::-1], 1, cloud_map.tobytes(), dtype="f4"
        )
        sun_map_texture.filter = (moderngl.LINEAR, moderngl.LINEAR)
        sun_map_texture.repeat_x = sun_map_texture.repeat_y = False
    (sun_map_texture or sky).use(11)
    prog["linearOutput"].value = linear
    prog["skyIrradiance"].write(diffuse_coefficients(radiance).tobytes())
    atlas, brdf = sky_specular_atlas(ctx, source)
    atlas.use(7)
    brdf.use(8)
    source.use(10)
    prog["skySpecularMap"].value = 7
    prog["skyBrdf"].value = 8
    prog["skyRadiance"].value = 10
    sky.use(0)
    prog["skyView"].value = 0
    sky.use(2)
    prog["detailMap"].value = 2
    prog["litterMap"].value = 12
    prog["litterScale"].value = detail_scale("leaf litter")
    prog["litterColour"].value = SURFACES["forest floor"][0]
    prog["litterTint"].value = DETAIL_TINTS["leaf litter"]
    prog["barkBoxBlend"].value = BARK_BOX_BLEND
    bed = world.water_bed
    transmission = bed.transmission if bed is not None else np.zeros((1, 1, 3))
    bed_map = ctx.texture(
        transmission.shape[1::-1], 3, transmission.astype("f2").tobytes(), dtype="f2"
    )
    bed_map.repeat_x = bed_map.repeat_y = False
    bed_map.use(5)
    prog["waterBedMap"].value = 5
    cover = world.terrain_cover
    terrain_map = ctx.texture(cover.pixels.shape[1::-1], 4, cover.pixels.tobytes())
    terrain_map.repeat_x = terrain_map.repeat_y = False
    terrain_map.build_mipmaps()
    terrain_map.use(13)
    prog["terrainCover"].value = 13
    prog["terrainTransform"].value = cover.transform
    prog["waterBedTransform"].value = bed.transform if bed is not None else (0, 0, 1, 1)
    prog["bedAlbedo"].value = BED_ALBEDO
    prog["waterIor"].value = WATER_IOR
    veil.use(1)
    prog["veil"].value = 1
    prog["eyeKm"].value = EARTH_RADIUS_KM + (world.eye[2] + world.source.datum) * 0.001
    ctx.enable(moderngl.DEPTH_TEST)
    crown_sites = []
    draws = [
        upload(
            ctx,
            prog,
            n,
            v,
            f,
            [(0, 0, 0, 0, 1)],
            ground_litter=world.ground_litter(n, v),
        )
        for n, v, f in chain(
            world.surfaces(crown_sites=crown_sites),
            world.grass_and_understory(config["seed"]),
        )
    ]
    for _, _, meshes, plants in world.woody_plants(config["seed"]):
        draws.extend(upload(ctx, prog, n, v, f, plants) for n, (v, f) in meshes.items())
    for _, meshes, plants in crown_batches(crown_sites):
        draws.extend(upload(ctx, prog, n, v, f, plants) for n, (v, f) in meshes.items())
    draws = [batch for batch in draws if batch is not None]
    shadow_maps, shadow_time = render_shadows(ctx, prog, draws, world)
    textures = {}
    planes = reflection_planes(world)
    reflection, reflection_ms = capture_reflections(
        ctx, prog, draws, textures, world, m, planes
    )
    target.use()
    prog["viewProjection"].write(m.T.astype("f4").tobytes())
    draw_surfaces(ctx, prog, draws, textures)
    for batch in draws:
        batch.release()
    for texture in textures.values():
        texture.release()
    for shadow in shadow_maps:
        shadow.release()
    reflection.release()
    bed_map.release()
    terrain_map.release()
    if sun_map_texture is not None:
        sun_map_texture.release()
    save_view(target, Path(output), linear=linear)
    target.release()
    prog.release()
    veil.release()
    sky.release()
    for texture in (atlas, brdf, source):
        texture.release()
    print(
        json.dumps(
            {
                "place": place["id"],
                "weather": world.weather,
                "device": ctx.info["GL_RENDERER"],
                "regional_crown_instances": sum(len(sites) for _, sites in crown_sites),
                "cloud_sun": cloud_sun,
                "spatial_cloud_sun": cloud_map is not None,
                "sky_radiance": str(sky_path) if sky_path is not None else "computed",
                "shadow_gpu_ms": round(shadow_time.elapsed * 1e-6, 2),
                "reflection_levels_m": [round(plane.height, 3) for plane in planes],
                "reflection_gpu_ms": round(reflection_ms, 2),
                "build_and_npy_s" if linear else "build_and_png_s": round(
                    perf_counter() - start, 2
                ),
            }
        ),
        flush=True,
    )
