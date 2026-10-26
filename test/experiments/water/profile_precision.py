"""Inspect decoded storage coordinates, without repairing or admitting native water."""

import time

import numpy as np

MODES = ("float64", "absolute32", "local32", "xy32-z64", "xy64-z32", "split32")


def encode(vertices, mode, origin):
    """Decode in Float64; split32 does not simulate GPU high/low arithmetic."""
    if mode not in MODES:
        raise ValueError("unknown coordinate representation")
    vertices, origin = np.asarray(vertices, float), np.asarray(origin, float)
    if (
        vertices.ndim != 2
        or vertices.shape[1] != 3
        or origin.shape != (3,)
        or not np.isfinite(vertices).all()
        or not np.isfinite(origin).all()
    ):
        raise ValueError("finite N by 3 vertices and explicit XYZ datum required")
    if mode == "float64":
        return vertices.copy(), vertices.nbytes
    datum = np.zeros(3) if mode == "absolute32" else origin
    local = vertices - datum
    with np.errstate(over="ignore", invalid="ignore"):
        high = local.astype(np.float32)
        decoded = high.astype(float)
        payload = high.nbytes + (0 if mode == "absolute32" else datum.nbytes)
        if mode == "split32":
            low = (local - decoded).astype(np.float32)
            decoded += low.astype(float)
            payload += low.nbytes
        elif mode == "xy32-z64":
            decoded[:, 2] = local[:, 2]
            payload += len(vertices) * 4
        elif mode == "xy64-z32":
            decoded[:, :2] = local[:, :2]
            payload += len(vertices) * 8
    decoded += datum
    if mode == "xy32-z64":
        decoded[:, 2] = vertices[:, 2]
    elif mode == "xy64-z32":
        decoded[:, :2] = vertices[:, :2]
    return decoded, payload


def determinant(vertices, faces):
    points = vertices[faces, :2]
    u, v = points[:, 1] - points[:, 0], points[:, 2] - points[:, 0]
    return u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]


def gradients(vertices, faces):
    """Independent explicit edge-equation inverse, not solver coefficients."""
    points = vertices[faces]
    u, v = points[:, 1] - points[:, 0], points[:, 2] - points[:, 0]
    det = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]
    return np.c_[
        (u[:, 2] * v[:, 1] - v[:, 2] * u[:, 1]) / det,
        (u[:, 0] * v[:, 2] - v[:, 0] * u[:, 2]) / det,
    ]


def inspect(original, decoded, faces, flow, rows, *, pins=None, down=0.1, cross=0.005):
    """Test the same finite directions and pins on the complete decoded faces."""
    report = {"passes_finite_precision_checks": False}
    if not np.isfinite(decoded).all():
        return dict(report, reason="nonfinite_decoded_coordinates")
    before, after = determinant(original, faces), determinant(decoded, faces)
    collapsed = int(np.count_nonzero(after == 0))
    reversed_faces = int(np.count_nonzero(before * after < 0))
    report.update(
        collapsed_faces=collapsed,
        reversed_faces=reversed_faces,
        maximum_coordinate_error_m=np.max(np.abs(decoded - original), axis=0).tolist(),
        maximum_pin_error_m=max(
            (abs(float(decoded[i, 2]) - height) for i, height in (pins or {}).items()),
            default=0.0,
        ),
    )
    if np.any(before == 0):
        raise ValueError("nondegenerate reference faces required")
    if collapsed or reversed_faces:
        return dict(report, reason="decoded_face_topology_changed")
    gradient = gradients(decoded, faces)
    along = np.einsum("fi,fi->f", gradient, flow)
    across = np.einsum("fi,fi->f", gradient, np.c_[-flow[:, 1], flow[:, 0]])
    direction = rows[:, 1:3] / np.linalg.norm(rows[:, 1:3], axis=1)[:, None]
    directed = np.einsum("fi,fi->f", gradient[rows[:, 0].astype(int)], direction)
    per_face = np.maximum.reduce((along, -along - down, np.abs(across) - cross))
    per_source = np.maximum(directed - rows[:, 4], rows[:, 3] - directed)
    worst = int(np.argmax(per_face))
    violation = max(float(per_face.max()), float(per_source.max(initial=0)), 0.0)
    report.update(
        maximum_grade_violation_m_per_m=violation,
        face_violations=int(np.count_nonzero(per_face > 1e-7)),
        source_face_violations=int(np.count_nonzero(per_source > 1e-7)),
        worst_base_face=worst,
        worst_base_face_grade_violation_m_per_m=max(float(per_face[worst]), 0.0),
        maximum_gradient_change_m_per_m=float(
            np.max(np.abs(gradient - gradients(original, faces)))
        ),
    )
    valid = violation <= 1e-7 and report["maximum_pin_error_m"] <= 1e-7
    return dict(
        report,
        passes_finite_precision_checks=valid,
        reason="" if valid else "decoded_gradient_or_pin_violation",
    )


def compare(vertices, faces, flow, rows, *, pins=None):
    origin = (vertices.min(axis=0) + vertices.max(axis=0)) / 2
    reports, decoded = {}, {}
    for mode in MODES:
        start = time.perf_counter()
        points, size = encode(vertices, mode, origin)
        converted = time.perf_counter()
        report = inspect(vertices, points, faces, flow, rows, pins=pins)
        report.update(
            conversion_ms=(converted - start) * 1000,
            conversion_and_inspection_ms=(time.perf_counter() - start) * 1000,
            vertices_converted=len(vertices),
            base_faces_checked=len(faces),
            source_face_rows_checked=len(rows),
            modeled_coordinate_payload_bytes=size,
            decoded_array_bytes=points.nbytes,
        )
        reports[mode], decoded[mode] = report, points
    return origin, reports, decoded
