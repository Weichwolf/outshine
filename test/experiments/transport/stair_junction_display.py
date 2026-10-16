"""Actual indexed recipe body in wide and local diagnostic 720p research views."""

import json

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from transport_surface_experiment import camera_settings


def display(args, body, data):
    camera = camera_settings(json.loads(args.palette_scene.read_text()))
    palette = np.array(camera["colours"])
    empty = {
        "vertices": np.empty((0, 3)),
        "triangles": np.empty((0, 3), int),
        "normals": np.empty((0, 3)),
        "materials": np.empty(0, int),
        "face_base_colour_linear_rgba": np.empty((0, 4)),
    }
    arrays = append(empty, [(body, 3)], palette)
    top_count = len(data["top_faces"])
    is_stair = (data["face_sources"] & 1) != 0
    selected = np.r_[is_stair, is_stair, data["wall_riser"] == 1]
    if len(selected) != len(body.triangles) or 2 * top_count > len(selected):
        raise ValueError("actual stair display cap/wall ownership differs")
    arrays["materials"][selected] = 10
    arrays["face_base_colour_linear_rgba"][selected] = palette[10]
    target = args.output / "display-body.npz"
    np.savez_compressed(target, **arrays)
    camera.update(profile=[1280, 720], samples=16, detail_view=True)
    views = {
        "overview": ([440, -400, 500], [140, 95, 166], 500),
        "diagnostic": ([30, -45, 197], [0, -6, 166], 42),
    }
    for name, (eye, point, span) in views.items():
        row = dict(
            camera,
            id="Wien-stair-junction-" + name,
            eye=eye,
            target=point,
            span_m=span,
            mesh=str(target),
            mesh_sha256=digest(target),
            output=str(args.output / (name + ".png")),
        )
        (args.output / (name + "-scene.json")).write_text(
            json.dumps(row, indent=2) + "\n"
        )
    return {
        "mesh_sha256": digest(target),
        "palette_scene_sha256": digest(args.palette_scene),
        "views": list(views),
        "scope": "Only actual four-way recipe, without inferred surrounding city or DEM. Diagnostic crop is local, not a presentation gallery.",
    }
