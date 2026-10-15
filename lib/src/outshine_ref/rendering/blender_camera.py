"""Shared Blender cameras; orthographic span is horizontal, perspective FOV vertical."""

import json
import math
from pathlib import Path

import numpy as np


def configure_camera(configuration):
    import bpy
    from mathutils import Matrix, Vector

    bpy.ops.object.camera_add(location=configuration["eye"])
    camera = bpy.context.object
    if "camera_basis" in configuration:
        camera.rotation_euler = Matrix(configuration["camera_basis"]).to_euler()
    else:
        camera.rotation_euler = (
            (Vector(configuration["target"]) - camera.location)
            .to_track_quat("-Z", "Y")
            .to_euler()
        )
    if configuration.get("projection") == "perspective":
        camera.data.type = "PERSP"
        camera.data.sensor_fit = "VERTICAL"
        camera.data.sensor_height = 32.0
        camera.data.lens = 16.0 / math.tan(
            math.radians(configuration["vertical_fov_deg"]) / 2
        )
        camera.data.clip_start = configuration["clip_near_m"]
        camera.data.clip_end = configuration["clip_far_m"]
    else:
        camera.data.type = "ORTHO"
        camera.data.sensor_fit = "HORIZONTAL"
        camera.data.ortho_scale = configuration["span_m"]
        camera.data.clip_end = 10000
    return camera


def verify_place_camera(configuration, projection, view, vertices):
    eye = np.asarray(configuration["eye"], dtype=float)
    forward = np.asarray(configuration["target"], dtype=float) - eye
    forward /= np.linalg.norm(forward)
    if "camera_basis" in configuration:
        up = np.asarray(configuration["camera_basis"])[:, 1]
    else:
        right = np.cross(forward, [0.0, 0.0, 1.0])
        right /= np.linalg.norm(right)
        up = np.cross(right, forward)
    extent = math.tan(math.radians(configuration["vertical_fov_deg"]) / 2)
    probes = eye + 1000 * np.array(
        [forward, forward + up * extent, forward - up * extent]
    )
    clip = np.c_[probes, np.ones(3)] @ view.T @ projection.T
    projected = clip[:, :2] / clip[:, 3:4]
    if not np.allclose(
        projected, [[0.0, 0.0], [0.0, 1.0], [0.0, -1.0]], rtol=0, atol=2e-5
    ):
        raise ValueError(
            "Blender camera disagrees with declared Place pose/vertical FOV"
        )
    positions = np.c_[vertices, np.ones(len(vertices))] @ view.T @ projection.T
    visible = (positions[:, 3] > 0) & np.all(
        np.abs(positions[:, :2]) <= positions[:, 3:4], axis=1
    )
    if not visible.any():
        raise ValueError("no generated vertex is visible from the declared camera")
    proof = {
        "projection": configuration["projection"],
        "eye": configuration["eye"],
        "target": configuration["target"],
        "vertical_fov_deg": configuration["vertical_fov_deg"],
        "profile": configuration["profile"],
        "visible_input_vertices": int(visible.sum()),
        "vertical_ndc_probes": projected.tolist(),
        "projection_matrix": projection.tolist(),
        "view_matrix": view.tolist(),
    }
    path = Path(configuration["output"]).with_name(configuration["id"] + "-camera.json")
    path.write_text(json.dumps(proof, indent=2) + "\n")
    return proof


def verify_camera(configuration, projection, view, vertices):
    if configuration["projection"] == "perspective":
        return verify_place_camera(configuration, projection, view, vertices)
    if configuration["projection"] != "orthographic":
        raise ValueError("Declared perspective or orthographic projection required")
    eye = np.asarray(configuration["eye"], dtype=float)
    forward = np.asarray(configuration["target"], dtype=float) - eye
    forward /= np.linalg.norm(forward)
    if "camera_basis" in configuration:
        basis = np.asarray(configuration["camera_basis"])
        right, up = basis[:, 0], basis[:, 1]
    else:
        right = np.cross(forward, [0.0, 0.0, 1.0])
        right /= np.linalg.norm(right)
        up = np.cross(right, forward)
    half_width = configuration["span_m"] / 2
    width, height = configuration["profile"]
    half_height = half_width * height / width
    centre = np.asarray(configuration["target"], dtype=float)
    probes = np.array(
        [
            centre,
            centre + right * half_width,
            centre - right * half_width,
            centre + up * half_height,
            centre - up * half_height,
        ]
    )
    clip = np.c_[probes, np.ones(5)] @ view.T @ projection.T
    projected = clip[:, :2] / clip[:, 3:4]
    if not np.allclose(
        projected, [[0, 0], [1, 0], [-1, 0], [0, 1], [0, -1]], rtol=0, atol=2e-5
    ):
        raise ValueError("Blender camera disagrees with declared orthographic span")
    positions = np.c_[vertices, np.ones(len(vertices))] @ view.T @ projection.T
    inside = np.all(np.abs(positions[:, :2] / positions[:, 3:4]) <= 1.00001, axis=1)
    if not inside.any() or (
        not configuration.get("detail_view", False) and not inside.all()
    ):
        raise ValueError("Input geometry is outside the declared orthographic camera")
    proof = {
        "projection": "orthographic",
        "eye": configuration["eye"],
        "target": configuration["target"],
        "span_m": configuration["span_m"],
        "profile": configuration["profile"],
        "visible_input_vertices": int(inside.sum()),
        "ndc_probes": projected.tolist(),
        "projection_matrix": projection.tolist(),
        "view_matrix": view.tolist(),
    }
    path = Path(configuration["output"]).with_name(configuration["id"] + "-camera.json")
    path.write_text(json.dumps(proof, indent=2) + "\n")
    return proof
