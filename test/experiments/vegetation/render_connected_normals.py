"""Saved-host unit-light shader checks, not a material/BRDF or transmission renderer."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import bpy
import numpy as np
from connected_normal_material import check_graph, material, source_attribute
from connected_reader import read_host
from outshine_ref.rendering.blender_camera import configure_camera
from outshine_ref.rendering.blender_mesh import mesh_object
from preview_shadows import configure_preview


def configure(direction, centre, span, *, width=2560, height=1440, samples=32):
    scene = bpy.context.scene
    configure_preview(scene)
    scene.eevee.taa_render_samples = samples
    scene.camera = configure_camera(
        {
            "projection": "orthographic",
            "eye": (centre + 3 * direction).tolist(),
            "target": centre.tolist(),
            "span_m": span,
        }
    )
    scene.camera.data.clip_start, scene.camera.data.clip_end = 0.01, 10
    scene.world = bpy.data.worlds.new("black unit-response background")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (
        0,
        0,
        0,
        1,
    )
    scene.view_settings.view_transform = "Raw"
    scene.view_settings.exposure, scene.view_settings.gamma = 0, 1
    scene.render.resolution_x, scene.render.resolution_y = width, height
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    return scene


def scene_objects(source, surfaces, direction, light):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    d = np.asarray(direction, float)
    d /= np.linalg.norm(d)
    side = np.cross((0, 0, 1), d)
    side /= np.linalg.norm(side)
    up = np.cross(d, side)
    points = np.vstack((source["vertices"], *[s["mesh"]["vertices"] for s in surfaces]))
    centre = (points.max(axis=0) + points.min(axis=0)) / 2
    distance = float(np.linalg.norm(np.ptp(points, axis=0))) * 1.15
    for row, sign in enumerate((1, -1)):
        shift = (0.5 - row) * distance * up
        src = material(f"source row{row}", sign * light)
        check_graph(src, raw=False)
        obj = mesh_object(
            f"source row{row}",
            source["vertices"] - distance * side / 2 + shift,
            source["faces"],
            source["normals"],
            src,
            True,
        )
        source_attribute(obj, source["normals"])
        for index, s in enumerate(surfaces):
            # Constant diagnostic light transformed into each original blade frame.
            surface = material(
                f"connected row{row} leaf{index}",
                sign * light @ s["basis"],
                s["normal"],
            )
            check_graph(surface, raw=True)
            m = s["mesh"]
            mesh_object(
                f"connected row{row} leaf{index}",
                m["vertices"] + distance * side / 2 + shift,
                m["faces"],
                m["normals"],
                surface,
                True,
                uv=m["uv"],
            )
    return configure(d, centre, distance * 3.2), {
        "camera_direction": d.tolist(),
        "light_host": light.tolist(),
        "span_m": distance * 3.2,
        "target": centre.tolist(),
        "layout": "left original; right connected32; top +light; bottom -light",
        "source_triangles_per_subject": len(source["faces"]),
        "candidate_triangles_per_subject": sum(
            len(s["mesh"]["faces"]) for s in surfaces
        ),
    }


def calibrate(output):
    """Known signed vector/mass checks both shader paths and backside independence."""
    from PIL import Image

    records = []
    vertices = np.array(
        ((-0.5, -0.5, 0), (0.5, -0.5, 0), (0.5, 0.5, 0), (-0.5, 0.5, 0))
    )
    faces = np.array(((0, 1, 2), (0, 2, 3)))
    normals = np.tile((0, 0, 1.0), (6, 1))
    uv = vertices[:, :2] + 0.5
    encoded = np.full((4, 4, 4), 0.25)
    encoded[..., 2], encoded[..., 3] = 0.375, 0.5  # raw moment (0,0,.25), mass .5
    for raw, sign in ((False, 1), (False, -1), (True, 1), (True, -1)):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        surface = material("known calibration", (0, 0, sign), encoded if raw else None)
        check_graph(surface, raw=raw)
        obj = mesh_object("calibration", vertices, faces, normals, surface, True, uv=uv)
        source_attribute(obj, normals)
        scene = configure(
            np.array((0, 0, -1.0)), np.zeros(3), 1.2, width=128, height=128, samples=64
        )
        path = output / f"calibration-{'raw' if raw else 'source'}-{sign}.png"
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
        value = float(np.asarray(Image.open(path))[32:96, 32:96, :3].mean() / 255)
        expected = (0.25 if raw else 1.0) if sign > 0 else 0.0
        if abs(value - expected) > 0.02:
            raise AssertionError(
                f"rendered signed-normal/mass calibration {value} != {expected}"
            )
        records.append(
            {"raw": raw, "sign": sign, "actual": value, "expected": expected}
        )
    return records


def render(inputs, output, node):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    start = time.perf_counter()
    calibration = calibrate(output)
    source, surfaces = read_host(inputs, node)
    light = np.array((0.8, -0.3, 0.12))
    light /= np.linalg.norm(light)
    records = []
    for name, direction in (
        ("oblique", (np.cos(1.8), np.sin(1.8), 0.8)),
        ("grazing", (np.cos(0.4), np.sin(0.4), 0.05)),
    ):
        scene, record = scene_objects(source, surfaces, direction, light)
        scene.render.filepath = str(output / (name + ".png"))
        before = time.perf_counter()
        bpy.ops.render.render(write_still=True)
        records.append(
            record | {"view": name, "render_s": time.perf_counter() - before}
        )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "host": node,
        "resolution": [2560, 1440],
        "blender": bpy.app.version_string,
        "samples": 32,
        "calibration": calibration,
        "cases": records,
        "wall_s": time.perf_counter() - start,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Software EEVEE Raw linear unit-cosine emission, original signed source corner normals; connected32 object-frame premultiplied raw moments, no moment normalization, channel-packed Float32 linear images. Same saved source/poses, +light/-light. Not calibrated R/T, BRDF, true transmission, lighting/shadows, native/GPU performance or complete LOD admission.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for key in ("inputs", "output"):
        p.add_argument("--" + key, type=Path, required=True)
    p.add_argument("--host", type=int, default=17)
    a = p.parse_args()
    render(a.inputs, a.output, a.host)
