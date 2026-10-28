"""Closed hollow rectangular rails in explicit planes, with real sealed ends."""

import numpy as np
import shapely
from indoor_mesh_checks import closed_mesh


def hollow_sweep(
    path, width_m, wall_m, basis=None, origin=None, *, closed=False, cap_m=None
):
    path = np.asarray(path, dtype=float)
    basis = np.eye(3) if basis is None else np.asarray(basis, dtype=float)
    origin = np.zeros(3) if origin is None else np.asarray(origin, dtype=float)
    cap_m = wall_m if cap_m is None else cap_m
    if (
        path.ndim != 2
        or path.shape[1] != 2
        or not 2 <= len(path) <= 1024
        or basis.shape != (3, 3)
        or origin.shape != (3,)
        or not np.isfinite((width_m, wall_m, cap_m)).all()
        or not np.isfinite(path).all()
        or not np.isfinite(basis).all()
        or not np.isfinite(origin).all()
        or not 0 < 2 * wall_m < width_m
        or cap_m <= 0
    ):
        raise ValueError(
            "Bounded finite rail path/frame and hollow dimensions required"
        )
    if not np.allclose(basis.T @ basis, np.eye(3), atol=1e-12, rtol=0):
        raise ValueError("Orthonormal rail basis required")
    if closed and np.array_equal(path[0], path[-1]):
        path = path[:-1]
    segments = np.roll(path, -1, axis=0) - path if closed else np.diff(path, axis=0)
    lengths = np.linalg.norm(segments, axis=1)
    centreline = shapely.LineString(np.vstack((path, path[0])) if closed else path)
    band = centreline.buffer(width_m / 2, cap_style="flat", join_style="mitre")
    band_error = abs(float(band.area) - width_m * float(lengths.sum()))
    band_arithmetic = 256 * np.spacing(max(1.0, float(abs(path).max()))) * band.length
    if (
        np.any(lengths <= max(2 * width_m, 2 * cap_m))
        or not centreline.is_simple
        or band_error > band_arithmetic
    ):
        raise ValueError("Resolved simple rail intervals required")
    tangent = segments / lengths[:, None]
    normals = np.column_stack((-tangent[:, 1], tangent[:, 0]))
    miter = np.empty_like(path)
    if closed:
        previous, following = np.roll(normals, 1, axis=0), normals
        denom = 1 + np.sum(np.roll(tangent, 1, axis=0) * tangent, axis=1)
        if np.any(denom <= 0):
            raise ValueError("Non-antipodal rail corners required")
        miter = (previous + following) / denom[:, None]
    else:
        miter[0], miter[-1] = normals[0], normals[-1]
        denom = 1 + np.sum(tangent[:-1] * tangent[1:], axis=1)
        if np.any(denom <= 0):
            raise ValueError("Non-antipodal rail corners required")
        miter[1:-1] = (normals[:-1] + normals[1:]) / denom[:, None]
    if np.linalg.norm(miter, axis=1).max() > np.sqrt(2) + 1e-12:
        raise ValueError("Rail corner exceeds explicit ninety-degree miter limit")
    half = width_m / 2
    outer = np.array([[-half, -half], [half, -half], [half, half], [-half, half]])
    inner = outer * (1 - 2 * wall_m / width_m)
    cross = np.concatenate((outer, inner))
    local = np.empty((len(path), 8, 3))
    local[:, :, :2] = path[:, None, :] + miter[:, None, :] * cross[None, :, 1, None]
    local[:, :, 2] = cross[None, :, 0]
    if not closed:
        local[0, 4:, :2] += tangent[0] * cap_m
        local[-1, 4:, :2] -= tangent[-1] * cap_m
    vertices = local.reshape(-1, 3) @ basis.T + origin
    faces = []
    pairs = (
        zip(range(len(path)), range(1, len(path)), strict=False)
        if not closed
        else zip(range(len(path)), np.roll(np.arange(len(path)), -1), strict=True)
    )
    for first, last in pairs:
        for k in range(4):
            next_k = (k + 1) % 4
            for inner_offset in (0, 4):
                a, b = 8 * first + k + inner_offset, 8 * first + next_k + inner_offset
                d, c = 8 * last + k + inner_offset, 8 * last + next_k + inner_offset
                two = [(a, d, c), (a, c, b)]
                faces.extend(
                    [tuple(reversed(face)) for face in two] if inner_offset else two
                )
    if not closed:
        for station, reverse in ((0, False), (len(path) - 1, True)):
            for offset in (0, 4):
                indices = 8 * station + offset + np.arange(4)
                two = [tuple(indices[[0, 1, 2]]), tuple(indices[[0, 2, 3]])]
                faces.extend(
                    [tuple(reversed(face)) for face in two]
                    if reverse != bool(offset)
                    else two
                )
    faces = np.asarray(faces, dtype=np.uint32)
    if np.linalg.det(basis) < 0:
        faces = faces[:, [0, 2, 1]]
    mesh = {
        "vertices": vertices,
        "faces": faces,
        "parts": np.zeros(len(faces), dtype=np.uint32),
    }
    audit = closed_mesh(mesh)
    length = float(lengths.sum())
    area = width_m**2 - (width_m - 2 * wall_m) ** 2
    expected = area * length + (
        0 if closed else 2 * cap_m * (width_m - 2 * wall_m) ** 2
    )
    numerical = (
        256
        * np.spacing(max(1.0, float(abs(vertices).max())))
        * audit["surface_area_m2"]
    )
    error = abs(audit["signed_volume_m3"] - expected)
    if error > numerical:
        raise ValueError("hollow_rail_volume_mismatch")
    return mesh, {
        "vertices": len(vertices),
        "triangles": len(faces),
        "payload_bytes": sum(a.nbytes for a in mesh.values()),
        "signed_volume_m3": audit["signed_volume_m3"],
        "expected_material_volume_m3": expected,
        "volume_error_m3": error,
        "volume_arithmetic_bound_m3": float(numerical),
        "maximum_miter_scale": float(np.linalg.norm(miter, axis=1).max()),
        "scope": "Actual closed hollow rectangular sweep with sealed open-path ends; uniform section and resolved planar miters, no statics/standard/nativeWorld certificate.",
    }
