"""Enlarged 3D coverage diagnostic: exact host, spatial cloud, nearest angular card."""

import argparse
import json
import resource
import time
from pathlib import Path

import bpy
import numpy as np
from group_cloud import greedy_cloud
from outshine_ref.rendering.blender_camera import configure_camera
from outshine_ref.rendering.blender_mesh import mesh_object
from preview_shadows import configure_preview


def coverage_material(name, alpha=None):
    result = bpy.data.materials.new(name)
    result.use_nodes = True
    result.surface_render_method = "DITHERED"
    nodes, links = result.node_tree.nodes, result.node_tree.links
    nodes.clear()
    emission = nodes.new("ShaderNodeEmission")
    emission.inputs["Color"].default_value = (0.055, 0.16, 0.017, 1)
    output = nodes.new("ShaderNodeOutputMaterial")
    if alpha is None:
        links.new(emission.outputs[0], output.inputs["Surface"])
        return result
    pixels = np.ones((*alpha.shape, 4), np.float32)
    pixels[..., 3] = alpha
    image = bpy.data.images.new(
        name, width=alpha.shape[1], height=alpha.shape[0], alpha=True, float_buffer=True
    )
    image.colorspace_settings.name = "Non-Color"
    image.alpha_mode = "CHANNEL_PACKED"
    image.pixels.foreach_set(pixels.ravel())
    image.update()
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = image
    tex.interpolation = "Linear"
    tex.extension = "EXTEND"
    mix = nodes.new("ShaderNodeMixShader")
    transparent = nodes.new("ShaderNodeBsdfTransparent")
    links.new(tex.outputs["Alpha"], mix.inputs[0])
    links.new(transparent.outputs[0], mix.inputs[1])
    links.new(emission.outputs[0], mix.inputs[2])
    links.new(mix.outputs[0], output.inputs["Surface"])
    return result


def render(inputs, output, host):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private rendering must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    began = time.perf_counter()
    folder = inputs / f"host-{host}"
    with np.load(folder / "source.npz", allow_pickle=False) as z:
        source = dict(z)
    with np.load(folder / "views64.npz", allow_pickle=False) as z:
        views = dict(z)
    charts = greedy_cloud(source, size=16)
    records = []
    for name, direction in (
        ("oblique", (np.cos(1.8), np.sin(1.8), 0.8)),
        ("grazing", (np.cos(0.4), np.sin(0.4), 0.05)),
    ):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        d = np.asarray(direction, float)
        d /= np.linalg.norm(d)
        side = np.cross((0, 0, 1), d)
        side /= np.linalg.norm(side)
        centre = (source["vertices"].max(axis=0) + source["vertices"].min(axis=0)) / 2
        distance = float(np.linalg.norm(np.ptp(source["vertices"], axis=0))) * 1.15
        # Parallel orthographic rays and equal translations keep the same viewpoint.
        exact = mesh_object(
            "exact coarse host",
            source["vertices"] - distance * side,
            source["faces"],
            source["normals"],
            coverage_material("exact"),
            True,
        )
        for i, chart in enumerate(charts):
            mesh = chart.mesh
            mesh_object(
                f"spatial {i}",
                mesh["vertices"],
                mesh["faces"],
                np.broadcast_to(chart.frame[:, 2], (4, 3)),
                coverage_material(f"spatial alpha {i}", chart.colour[..., 3]),
                uv=mesh["uv"],
            )
        index = int(np.argmax(views["frames"][:, :, 2] @ d))
        vertices = views["vertices"][index]
        quad = np.array(((0, 1, 2), (0, 2, 3)), np.int32)
        uv = np.array(((0, 0), (1, 0), (1, 1), (0, 1)), float)
        mesh_object(
            "nearest angular card",
            vertices + distance * side,
            quad,
            np.broadcast_to(views["frames"][index, :, 2], (4, 3)),
            coverage_material("angular alpha", views["colour"][index, ..., 3]),
            uv=uv,
        )
        scene = bpy.context.scene
        configure_preview(scene)
        scene.eevee.taa_render_samples = 16
        scene.camera = configure_camera(
            {
                "projection": "orthographic",
                "eye": (centre + 3 * d).tolist(),
                "target": centre.tolist(),
                "span_m": distance * 3.1,
            }
        )
        scene.camera.data.clip_start = 0.01
        scene.camera.data.clip_end = 10
        scene.world = bpy.data.worlds.new("neutral diagnostic background")
        scene.world.use_nodes = True
        scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (
            0.2,
            0.22,
            0.24,
            1,
        )
        scene.view_settings.view_transform = "Standard"
        scene.render.resolution_x, scene.render.resolution_y = 2560, 1440
        scene.render.resolution_percentage = 100
        scene.render.image_settings.file_format = "PNG"
        scene.render.filepath = str(output / (name + ".png"))
        start = time.perf_counter()
        bpy.ops.render.render(write_still=True)
        records.append(
            {
                "view": name,
                "direction": d.tolist(),
                "angular_index": index,
                "triangles": [len(exact.data.polygons), 2 * len(charts), 2],
                "render_s": time.perf_counter() - start,
            }
        )
    result = {
        "cases": records,
        "host_node": host,
        "blender": bpy.app.version_string,
        "resolution": [2560, 1440],
        "samples": 16,
        "wall_s": time.perf_counter() - began,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Enlarged unlit coverage-only 3D render, left exact8x1 host / middle spatial charts / right selected64-view card. Not calibrated material, light, wind, shadow, intended8-pixel footprint or whole-crown admission.",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for k in ("inputs", "output"):
        p.add_argument("--" + k, type=Path, required=True)
    p.add_argument("--host", type=int, default=17)
    a = p.parse_args()
    render(a.inputs, a.output, a.host)
