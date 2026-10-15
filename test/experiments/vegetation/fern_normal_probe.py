"""Actual EEVEE source-attribute/instance/bump pixels against planar analytic normals."""

import argparse
import json
import subprocess
from pathlib import Path

import bpy
import gpu
import numpy as np
from blender_instances import instance_group
from blender_preview import material
from fern_material import attach_normals, source_normal
from outshine_ref.rendering.blender_camera import configure_camera
from outshine_ref.rendering.blender_mesh import mesh_object
from preview_shadows import configure_preview
from scipy.spatial.transform import Rotation


def probe(output, name, normal, scale, bump=False):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    configure_preview(scene)
    scene.render.resolution_x, scene.render.resolution_y = 64, 64
    scene.render.resolution_percentage = 100
    scene.eevee.taa_render_samples = 4
    scene.render.image_settings.file_format = "OPEN_EXR"
    scene.render.image_settings.color_depth = "32"
    result = material("normal diagnostic", (0.1, 0.1, 0.1), 0.5)
    nodes, links = result.node_tree.nodes, result.node_tree.links
    normal = np.asarray(normal, float)
    normal /= np.linalg.norm(normal)
    socket = source_normal(result)
    if bump:
        uv = nodes.new("ShaderNodeTexCoord")
        split = nodes.new("ShaderNodeSeparateXYZ")
        links.new(uv.outputs["UV"], split.inputs[0])
        perturb = nodes.new("ShaderNodeBump")
        perturb.inputs["Strength"].default_value = 1
        perturb.inputs["Distance"].default_value = 0.004
        links.new(split.outputs["X"], perturb.inputs["Height"])
        links.new(socket, perturb.inputs["Normal"])
        socket = perturb.outputs["Normal"]
    encode = nodes.new("ShaderNodeVectorMath")
    encode.operation = "SCALE"
    encode.inputs["Scale"].default_value = 0.5
    links.new(socket, encode.inputs[0])
    add = nodes.new("ShaderNodeVectorMath")
    add.operation = "ADD"
    add.inputs[1].default_value = (0.5, 0.5, 0.5)
    links.new(encode.outputs[0], add.inputs[0])
    emit = nodes.new("ShaderNodeEmission")
    links.new(add.outputs[0], emit.inputs["Color"])
    links.new(emit.outputs[0], nodes["Material Output"].inputs["Surface"])
    vertices = np.array(
        ((-0.5, -0.5, 0), (0.5, -0.5, 0), (0.5, 0.5, 0), (-0.5, 0.5, 0))
    )
    obj = mesh_object(
        "source plane",
        vertices,
        np.array(((0, 1, 2), (0, 2, 3))),
        np.tile(normal, (6, 1)),
        result,
        corner_normals=True,
    )
    attach_normals(obj, np.tile(normal, (6, 1)))
    obj.data.uv_layers.new(name="UV").data.foreach_set(
        "uv",
        np.array(((0, 0), (1, 0), (1, 1), (0, 0), (1, 1), (0, 1)), np.float32).ravel(),
    )
    frame = Rotation.from_rotvec((0.2, -0.3, 0.4)).as_matrix()
    scale = np.asarray(scale, float)
    instance_group(
        obj,
        np.zeros((1, 3)),
        frame[None],
        scale[None],
        name="nonuniform actual instance",
    )
    scene.camera = configure_camera(
        {
            "projection": "orthographic",
            "eye": (frame @ np.array((0, 0, 5))).tolist(),
            "target": [0, 0, 0],
            "span_m": 4,
        }
    )
    path = output / (name + ".exr")
    scene.render.filepath = str(path.resolve())
    bpy.ops.render.render(write_still=True)
    image = bpy.data.images.load(str(path))
    pixels = np.empty(64 * 64 * 4, np.float32)
    image.pixels.foreach_get(pixels)
    actual = pixels.reshape(64, 64, 4)[32, 32, :3].astype(float) * 2 - 1
    if bump:
        expected = frame @ np.array((-0.004 / scale[0], 0, 1.0))
    else:
        expected = frame @ (normal / scale)
    expected /= np.linalg.norm(expected)
    return {
        "case": name,
        "source_normal": normal.tolist(),
        "scale": scale.tolist(),
        "bump": bump,
        "expected": expected.tolist(),
        "decoded_pixel": actual.tolist(),
        "max_abs_error": float(np.max(abs(actual - expected))),
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    rows = [
        probe(args.output, "uniform", (0.3, 0.2, 1), (1, 1, 1)),
        probe(args.output, "nonuniform", (0.3, 0.2, 1), (2, 0.7, 1.3)),
        probe(args.output, "nonuniform-bump", (0, 0, 1), (2, 0.7, 1.3), True),
    ]
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "renderer": gpu.platform.renderer_get(),
        "rows": rows,
        "limit": 2e-5,
        "scope": "64x64 linear32-bit EXR emission normals, three planar cases through actual GeometryNodes instances; no arbitrary curved/mip/native admission.",
    }
    (args.output / "probe.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)
    if any(row["max_abs_error"] > report["limit"] for row in rows):
        raise SystemExit(1)
