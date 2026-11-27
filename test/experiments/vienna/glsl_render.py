"""Render generated reference geometry with the shared Outshine GLSL on the hardware GPU."""

import argparse
import json
import re
import sys
from itertools import chain
from pathlib import Path
from time import perf_counter

import moderngl
import numpy as np
from catalog import HERE, ROOT, available_views, load_catalog, selected_views
from PIL import Image

sys.path.insert(0, str(ROOT / "lib/src"))
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
    WATER_MATERIALS,
    surface_properties,
)
from reference_scene import ReferenceScene
from sky_environment import (
    SKY_ENVIRONMENT_SIZE,
    diffuse_coefficients,
    environment_path,
    load_environment,
    save_environment,
)
from water_bed import BED_ALBEDO
from water_reflection import (
    LEVEL_TOLERANCE_M,
    REFLECTION_RANGE_M,
    camera_axes,
    reflection_planes,
)

REFLECTION_SIZE = (640, 360)
REGIONAL_SHADOW_SPAN = (4800.0, 18000.0)
LOCAL_SHADOW_SPAN = (96.0, 1024.0)

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


def atmosphere(ctx, target, world, right, up, *, environment=False, linear=False):
    prog = ctx.program(
        vertex_shader=SCREEN, fragment_shader=shader(HERE / "atmosphere.frag")
    )
    quad = ctx.vertex_array(prog, [])
    prog["linearOutput"].value = linear
    for key, value in {
        "eyeKm": 6360 + (world.eye[2] + world.source.datum) * 0.001,
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
    placeholder.release()
    quad.release()
    prog.release()
    veil, multi_scatter, sky_view = textures
    multi_scatter.release()
    return veil, sky_view


def export_sky(ctx, place, config, out):
    world = ReferenceScene(place, config)
    _, right, up = projection(world)
    target = ctx.simple_framebuffer(SKY_ENVIRONMENT_SIZE, components=4, dtype="f4")
    ctx.disable(moderngl.DEPTH_TEST)
    veil, sky = atmosphere(ctx, target, world, right, up, environment=True)
    width, height = target.size
    pixels = np.frombuffer(target.read(components=3, dtype="f4"), dtype="f4")
    path = environment_path(out, place)
    eye_m = world.eye[2] + world.source.datum
    save_environment(path, pixels.reshape(height, width, 3), world.sun, eye_m)
    for resource in (veil, sky, target):
        resource.release()
    print(
        json.dumps({"place": place["id"], "sky": str(path), "eye_m": eye_m}), flush=True
    )


def sky_specular_atlas(ctx, radiance):
    native_layout = ROOT / "src/render/stages/EnvironmentSpecularValues.inc"
    values = dict(re.findall(r"ENV_INT\((\w+), (\d+)\)", native_layout.read_text()))
    span = int(values["kEnvironmentSide"]) + 2 * int(values["kEnvironmentBorder"])
    source = ctx.texture(SKY_ENVIRONMENT_SIZE, 3, radiance.tobytes(), dtype="f4")
    source.repeat_x = True
    source.repeat_y = False
    source.filter = (moderngl.LINEAR, moderngl.LINEAR)
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
    for resource in (quad, prog, source):
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


def upload(ctx, program, name, vertices, faces, placements):
    faces = np.asarray(faces, dtype="i4")
    if not faces.size:
        return
    if faces.shape[1] == 4:
        faces = faces[:, [0, 1, 2, 0, 2, 3]]
    vertices = np.asarray(vertices, dtype="f4")
    points = ctx.buffer(vertices.tobytes())
    idx = ctx.buffer(faces.tobytes())
    instances = ctx.buffer(np.asarray(placements, dtype="f4").tobytes())
    content = [
        (points, "3f", "position"),
        (instances, "4f 1f /i", "offsetAngle", "scale"),
    ]
    buffers = [instances, idx, points]
    if name.endswith(" bark"):
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
    return name, len(placements), vao, tuple(buffers)


def shadow_projection(world, span, forward_offset):
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
    return light


def shadow_depth(ctx, prog, draws, light, size):
    texture = ctx.depth_texture((size, size))
    texture.repeat_x = texture.repeat_y = False
    texture.filter = (moderngl.LINEAR, moderngl.LINEAR)
    target = ctx.framebuffer(depth_attachment=texture)
    target.use()
    target.clear(depth=1)
    prog["viewProjection"].write(light)
    for name, count, vao, _ in draws:
        if name not in WATER_MATERIALS:
            vao.render(instances=count)
    target.release()
    return texture


def draw_surfaces(ctx, prog, draws, textures, *, capture=False):
    for name, count, vao, _ in draws:
        water = name in WATER_MATERIALS
        if capture and water:
            continue
        base, rgb, rough = surface_properties(name)
        prog["surface"].value = (*rgb, rough)
        prog["metalness"].value = METALLIC.get(name, 0.0)
        prog["diffuseTransmission"].value = DIFFUSE_TRANSMISSION.get(name, 0.0)
        prog["waterSurface"].value = water
        prog["glassIor"].value = GLASS_IOR if name == "glass" else 1.0
        detail = SURFACE_DETAIL.get(base)
        prog["hasDetail"].value = detail is not None
        prog["detailTint"].value = DETAIL_TINTS.get(detail, (1, 1, 1))
        if detail is not None:
            if detail not in textures:
                pixels = ground_detail(detail)
                texture = ctx.texture(pixels.shape[:2], 4, pixels.tobytes(), dtype="f4")
                texture.build_mipmaps()
                textures[detail] = texture
            textures[detail].use(2)
        vao.render(instances=count)


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
    image.resize((1280, 720), Image.Resampling.BOX).save(path.with_suffix(".png"))


def render(ctx, place, config, out, *, sky_dir=None, linear=False):
    start = perf_counter()
    world = ReferenceScene(place, config)
    target = ctx.simple_framebuffer(
        (2560, 1440), components=4, dtype="f4" if linear else "f1"
    )
    target.use()
    target.clear(depth=1)
    ctx.disable(moderngl.DEPTH_TEST)
    m, right, up = projection(world)
    veil, sky = atmosphere(ctx, target, world, right, up, linear=linear)
    prog = ctx.program(
        vertex_shader=shader(HERE / "reference.vert"),
        fragment_shader=shader(HERE / "reference.frag"),
    )
    prog["viewProjection"].write(m.T.astype("f4").tobytes())
    prog["eye"].value = tuple(world.eye)
    prog["sun"].value = tuple(world.sun)
    prog["linearOutput"].value = linear
    sky_path = environment_path(sky_dir, place) if sky_dir is not None else None
    prog["useSkyIrradiance"].value = sky_path is not None
    sky_tables = ()
    if sky_path is not None:
        radiance = load_environment(
            sky_path,
            world.sun,
            world.eye[2] + world.source.datum,
        )
        prog["skyIrradiance"].write(diffuse_coefficients(radiance).tobytes())
        sky_tables = sky_specular_atlas(ctx, radiance)
    atlas, brdf = sky_tables or (sky, sky)
    atlas.use(7)
    brdf.use(8)
    prog["skySpecularMap"].value = 7
    prog["skyBrdf"].value = 8
    sky.use(0)
    prog["skyView"].value = 0
    sky.use(2)
    prog["detailMap"].value = 2
    prog["detailScale"].value = (DETAIL_SPAN_M, MACRO_SPAN_M, HEIGHT_SPAN_M)
    prog["barkBoxBlend"].value = BARK_BOX_BLEND
    bed = world.water_bed
    transmission = bed.transmission if bed is not None else np.zeros((1, 1, 3))
    bed_map = ctx.texture(
        transmission.shape[1::-1], 3, transmission.astype("f2").tobytes(), dtype="f2"
    )
    bed_map.repeat_x = bed_map.repeat_y = False
    bed_map.use(5)
    prog["waterBedMap"].value = 5
    prog["waterBedTransform"].value = bed.transform if bed is not None else (0, 0, 1, 1)
    prog["bedAlbedo"].value = BED_ALBEDO
    veil.use(1)
    prog["veil"].value = 1
    prog["eyeKm"].value = 6360 + (world.eye[2] + world.source.datum) * 0.001
    ctx.enable(moderngl.DEPTH_TEST)
    draws = [
        upload(ctx, prog, n, v, f, [(0, 0, 0, 0, 1)])
        for n, v, f in chain(
            world.surfaces(), world.grass_and_understory(config["seed"])
        )
    ]
    for _, _, meshes, plants in world.woody_plants(config["seed"]):
        draws.extend(upload(ctx, prog, n, v, f, plants) for n, (v, f) in meshes.items())
    draws = [batch for batch in draws if batch is not None]
    # Bind complete textures on both units while rendering depth attachments.
    placeholder = ctx.depth_texture((1, 1))
    placeholder.use(3)
    placeholder.use(6)
    prog["shadowMap"].value = 3
    prog["localShadowMap"].value = 6
    regional_light = shadow_projection(world, REGIONAL_SHADOW_SPAN, 1000)
    local_light = shadow_projection(world, LOCAL_SHADOW_SPAN, 12)
    regional_light = regional_light.T.astype("f4").tobytes()
    local_light = local_light.T.astype("f4").tobytes()
    prog["shadowMatrix"].write(regional_light)
    prog["localShadowMatrix"].write(local_light)
    prog["regionalShadowSpan"].value = REGIONAL_SHADOW_SPAN
    prog["localShadowSpan"].value = LOCAL_SHADOW_SPAN
    prog["shadowPass"].value = True
    shadow_time = ctx.query(time=True)
    with shadow_time:
        shadow = shadow_depth(ctx, prog, draws, regional_light, 4096)
        local_shadow = shadow_depth(ctx, prog, draws, local_light, 2048)
    shadow.use(3)
    local_shadow.use(6)
    prog["shadowPass"].value = False
    textures = {}
    planes = reflection_planes(world)
    reflection, reflection_ms = capture_reflections(
        ctx, prog, draws, textures, world, m, planes
    )
    target.use()
    prog["viewProjection"].write(m.T.astype("f4").tobytes())
    draw_surfaces(ctx, prog, draws, textures)
    for _, _, vao, buffers in draws:
        vao.release()
        for buffer in buffers:
            buffer.release()
    for texture in textures.values():
        texture.release()
    placeholder.release()
    shadow.release()
    local_shadow.release()
    reflection.release()
    bed_map.release()
    save_view(target, out / place["id"], linear=linear)
    target.release()
    prog.release()
    veil.release()
    sky.release()
    for texture in sky_tables:
        texture.release()
    print(
        json.dumps(
            {
                "place": place["id"],
                "device": ctx.info["GL_RENDERER"],
                "sky_radiance": str(sky_path) if sky_path is not None else None,
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


if __name__ == "__main__":
    config = load_catalog()
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--place", action="append", choices=[v["id"] for v in available_views(config)]
    )
    parser.add_argument("--output", type=Path, default=ROOT / "build/shots/glsl")
    sky_args = parser.add_mutually_exclusive_group()
    sky_args.add_argument(
        "--export-sky", action="store_true", help="write scene-linear sky radiance only"
    )
    sky_args.add_argument("--sky-dir", type=Path, help="use exported sky lighting")
    parser.add_argument(
        "--linear", action="store_true", help="write scene-linear RGB NPY"
    )
    args = parser.parse_args()
    if args.export_sky and args.linear:
        parser.error("--linear exports the camera view. Use --export-sky for radiance.")
    with moderngl.create_standalone_context(require=410) as ctx:
        for place in selected_views(config, args.place):
            if args.export_sky:
                export_sky(ctx, place, config, args.output)
            else:
                render(
                    ctx,
                    place,
                    config,
                    args.output,
                    sky_dir=args.sky_dir,
                    linear=args.linear,
                )
