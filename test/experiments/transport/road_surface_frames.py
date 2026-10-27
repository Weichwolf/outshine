"""Recipe-derived Upper shading frames, independent of display triangles (#1531)."""

import numpy as np


def parameters(curve, stations, side):
    stations = np.asarray(stations, dtype=float)
    if (
        side not in ("left", "right")
        or stations.ndim != 1
        or not np.isfinite(stations).all()
        or np.any(stations < curve.stations[0])
        or np.any(stations > curve.stations[-1])
    ):
        raise ValueError(
            "Finite in-domain stations and explicit left/right side required"
        )
    # 1-u*k is the horizontal sweep Jacobian. Checking its whole interval
    # envelope catches folds between requested render stations as well.
    if np.any(curve.width * curve.curvature_max >= 1):
        raise ValueError("Singular or folded full-width analytic sweep")
    indices = np.clip(
        np.searchsorted(curve.stations, stations, side=side) - 1,
        0,
        len(curve.lengths) - 1,
    )
    distance = stations - curve.stations[indices]
    return stations, indices, distance, distance / curve.lengths[indices]


def differentials(curve, stations, *, side="right", lateral_m=None):
    """Return dP/ds and dP/du for east/north/height P(s,u).

    s is reference-line horizontal arc length, u is signed horizontal distance
    leftward. These coordinates are not surface arc lengths. Source boundaries
    use the requested one-sided analytic continuation, never triangle averaging.
    """
    _, i, distance, t = parameters(curve, stations, side)
    across = curve.across if lateral_m is None else np.asarray(lateral_m, dtype=float)
    if (
        across.ndim != 1
        or not np.isfinite(across).all()
        or np.any(abs(across) > curve.width)
    ):
        raise ValueError(
            "Finite lateral coordinates inside the full road width required"
        )
    heading = (
        curve.poses[i, 4]
        + curve.curvature_start[i] * distance
        + curve.curvature_rate[i] * distance**2 / 2
    )
    curvature = curve.curvature_start[i] + curve.curvature_rate[i] * distance
    grade = curve.height_at(i, t, derivative=1)
    crossfall = curve.crossfall(i, t)
    crossfall_rate = curve.crossfall(i, t, derivative=1)
    factor = 1 - curvature[:, None] * across
    longitudinal = np.empty((len(i), len(across), 3))
    longitudinal[:, :, 0] = factor * np.cos(heading[:, None])
    longitudinal[:, :, 1] = factor * np.sin(heading[:, None])
    longitudinal[:, :, 2] = grade[:, None] - across * crossfall_rate[:, None]
    lateral = np.broadcast_to(
        np.c_[-np.sin(heading), np.cos(heading), -crossfall][:, None, :],
        longitudinal.shape,
    )
    if not np.isfinite(longitudinal).all() or not np.isfinite(lateral).all():
        raise ValueError("Finite analytic surface differentials required")
    return longitudinal, lateral


def attributes(curve, stations, *, side="right", lateral_m=None):
    """Return Float32 normals/TBN tangents and Double metric chart coordinates.

    Tangent XYZ follows positive s; W=+1 reconstructs positive-u bitangent as
    cross(normal, tangent). The orthonormal frame does not encode UV stretch:
    differentials() supplies the complete nonorthogonal metric when needed.
    """
    longitudinal, lateral = differentials(
        curve, stations, side=side, lateral_m=lateral_m
    )
    tangent = unit(longitudinal)
    normal = unit(np.cross(tangent, unit(lateral)))
    handed = np.concatenate((tangent, np.ones(tangent.shape[:2] + (1,))), axis=2)
    metric = np.empty(tangent.shape[:2] + (2,))
    metric[:, :, 0] = np.asarray(stations)[:, None]
    metric[:, :, 1] = curve.across if lateral_m is None else lateral_m
    return {
        "normals_enh": normal.astype(np.float32),
        "tangents_enh": handed.astype(np.float32),
        "station_lateral_m": metric,
    }


def unit(vectors):
    scale = np.max(abs(vectors), axis=-1, keepdims=True)
    if not np.isfinite(scale).all() or np.any(scale == 0):
        raise ValueError("Finite nonzero surface frame vectors required")
    scaled = vectors / scale
    return scaled / np.linalg.norm(scaled, axis=-1, keepdims=True)


def triangle_normals(plan):
    """Area-weighted triangle control; no recipe data used by this opponent."""
    points = plan["positions_enh_m"].reshape(-1, 3).astype(float)
    faces = plan["indices"]
    triangles = points[faces]
    area = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normals = np.zeros_like(points)
    for corner in range(3):
        np.add.at(normals, faces[:, corner], area)
    lengths = np.linalg.norm(normals, axis=1)
    used = lengths > 0
    normals[used] /= lengths[used, None]
    # Zero-shoulder recipes retain unused duplicate vertices. Do not invent
    # normals for these; every referenced vertex must nevertheless be valid.
    if not np.all(used[np.unique(faces)]):
        raise ValueError("Degenerate referenced triangle-normal control")
    return normals.reshape(plan["positions_enh_m"].shape), used.reshape(-1, 4)
