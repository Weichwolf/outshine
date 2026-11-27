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
    WATER_MATERIALS,
)
from reference_scene import ReferenceScene
from water_bed import BED_ALBEDO
from water_reflection import (
    LEVEL_TOLERANCE_M,
    REFLECTION_RANGE_M,
    camera_axes,
    reflection_planes,
)

REFLECTION_SIZE = (640, 360)

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


def atmosphere(ctx, target, world, right, up):
    prog = ctx.program(
        vertex_shader=SCREEN, fragment_shader=shader(HERE / "atmosphere.frag")
    )
    quad = ctx.vertex_array(prog, [])
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
    prog["stage"].value = 3
    prog["extent"].value = target.size
    quad.render(vertices=3)
    placeholder.release()
    quad.release()
    prog.release()
    veil, multi_scatter, sky_view = textures
    multi_scatter.release()
    return veil, sky_view


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


def shadow_projection(world):
    right = np.cross([0.0, 0.0, 1.0], world.sun)
    if np.linalg.norm(right) < 1e-6:
        right = np.array([1.0, 0.0, 0.0])
    right /= np.linalg.norm(right)
    up = np.cross(world.sun, right)
    center = world.eye + 1000 * world.look
    light = np.eye(4)
    light[:3, :3] = np.array([right / 2400, up / 2400, -world.sun / 9000])
    light[:3, 3] = -light[:3, :3] @ center
    return light


def draw_surfaces(ctx, prog, draws, textures, *, capture=False):
    for name, count, vao, _ in draws:
        water = name in WATER_MATERIALS
        if capture and water:
            continue
        rgb, rough = SURFACES[name]
        prog["surface"].value = (*rgb, rough)
        prog["metalness"].value = METALLIC.get(name, 0.0)
        prog["diffuseTransmission"].value = DIFFUSE_TRANSMISSION.get(name, 0.0)
        prog["waterSurface"].value = water
        prog["glassIor"].value = GLASS_IOR if name == "glass" else 1.0
        detail = SURFACE_DETAIL.get(name)
        prog["hasDetail"].value = detail is not None
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


def render(ctx, place, config, out):
    start = perf_counter()
    world = ReferenceScene(place, config)
    target = ctx.simple_framebuffer((2560, 1440), components=4)
    target.use()
    target.clear(depth=1)
    ctx.disable(moderngl.DEPTH_TEST)
    m, right, up = projection(world)
    veil, sky = atmosphere(ctx, target, world, right, up)
    prog = ctx.program(
        vertex_shader=shader(HERE / "reference.vert"),
        fragment_shader=shader(HERE / "reference.frag"),
    )
    prog["viewProjection"].write(m.T.astype("f4").tobytes())
    prog["eye"].value = tuple(world.eye)
    prog["sun"].value = tuple(world.sun)
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
    for _, _, meshes, plants in world.trees(config["seed"]):
        draws.extend(upload(ctx, prog, n, v, f, plants) for n, (v, f) in meshes.items())
    draws = [batch for batch in draws if batch is not None]
    shadow = ctx.depth_texture((4096, 4096))
    shadow.repeat_x = shadow.repeat_y = False
    shadow.filter = (moderngl.LINEAR, moderngl.LINEAR)
    depth = ctx.framebuffer(depth_attachment=shadow)
    depth.use()
    depth.clear(depth=1)
    # Bind a separate complete depth texture while writing the shadow attachment.
    placeholder = ctx.depth_texture((1, 1))
    placeholder.use(3)
    prog["shadowMap"].value = 3
    light = shadow_projection(world).T.astype("f4").tobytes()
    prog["shadowMatrix"].write(light)
    prog["viewProjection"].write(light)
    prog["shadowPass"].value = True
    for name, count, vao, _ in draws:
        if name not in WATER_MATERIALS:
            vao.render(instances=count)
    shadow.use(3)
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
    depth.release()
    placeholder.release()
    shadow.release()
    reflection.release()
    bed_map.release()
    im = Image.frombytes(
        "RGB", target.size, target.read(components=3, alignment=1)
    ).transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    im = im.resize((1280, 720), Image.Resampling.BOX)
    out.mkdir(parents=True, exist_ok=True)
    im.save(out / (place["id"] + ".png"))
    target.release()
    prog.release()
    veil.release()
    sky.release()
    print(
        json.dumps(
            {
                "place": place["id"],
                "device": ctx.info["GL_RENDERER"],
                "reflection_levels_m": [round(plane.height, 3) for plane in planes],
                "reflection_gpu_ms": round(reflection_ms, 2),
                "build_and_png_s": round(perf_counter() - start, 2),
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
    args = parser.parse_args()
    with moderngl.create_standalone_context(require=410) as ctx:
        for place in selected_views(config, args.place):
            render(ctx, place, config, args.output)
