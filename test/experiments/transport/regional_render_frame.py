"""Explicit Double frame subtraction shared by the two research renderers."""

import hashlib

import numpy as np


def vector(value):
    result = np.asarray(value, dtype=float)
    if result.shape != (3,) or not np.isfinite(result).all():
        raise ValueError("finite explicit three-dimensional frame vector required")
    return result


def render_camera(row):
    if "render_origin_m" not in row:
        return row
    origin = vector(row["render_origin_m"])
    result = dict(row)
    for key in ("eye", "target"):
        result[key] = (vector(row[key]) - origin).tolist()
    return result


def bind_positions(obj, vertices, row, render_origin=None):
    origin = vector(row.get("origin_m", [0, 0, 0]))
    frame = vector([0, 0, 0] if render_origin is None else render_origin)
    if row.get("encoded_float32") and (
        vertices.dtype != np.dtype("f4") or "origin_m" not in row
    ):
        raise ValueError(
            "actual Float32 payload and its explicit owner origin required"
        )
    delta = origin - frame
    obj.location = delta.tolist()
    actual_delta = np.asarray(obj.location)
    if not np.isfinite(actual_delta).all() or not np.array_equal(
        actual_delta, delta.astype("f4")
    ):
        raise ValueError("mesh origin differs from Double subtraction before cast")
    actual = np.empty(vertices.shape, dtype="f4")
    obj.data.vertices.foreach_get("co", actual.ravel())
    if not np.isfinite(actual).all() or not np.array_equal(
        actual.view("u4"), vertices.astype("f4").view("u4")
    ):
        raise ValueError("Blender changed actual vertex payload")
    proof = {
        "input_mesh_sha256": row["mesh_sha256"],
        "origin_m": origin.tolist(),
        "origin_minus_render_origin_f32_m": actual_delta.tolist(),
        "actual_vertex_payload_sha256": hashlib.sha256(actual.tobytes()).hexdigest(),
    }
    # Legacy camera verification keeps its original binary64 input convention.
    relative = (
        vertices
        if render_origin is None and "origin_m" not in row
        else actual.astype(float) + actual_delta
    )
    return relative, proof


def scene_bindings(row, imported, camera):
    if "render_origin_m" not in row:
        return {}
    import bpy

    for part in imported.get("parts", [imported]):
        obj = bpy.data.objects[part.get("id", row["id"])]
        part["regional_binding"]["actual_model_matrix_f32"] = np.asarray(
            obj.matrix_world
        ).tolist()
    return {
        "regional_frame": {
            "render_origin_m": row["render_origin_m"],
            "input_eye": row["eye"],
            "input_target": row["target"],
            "actual_camera_translation_f32_m": list(camera.location),
            "scope": "Blender CPU Float32 mesh/model/camera bindings and actual Eevee images; no native shader/motion-vector/device admission.",
        }
    }
