"""Physical plane heights and geodetic grades in an explicit WGS84 frame (#760).

The input height is a declared ellipsoidal model coordinate, not an observed
orthometric/ASL-to-ellipsoid conversion. No source/solver constraints are changed.
"""

import numpy as np
from outshine_ref.places.camera import axes, ecef

WGS84_A = 6378137.0
WGS84_E2 = 6.6943799901413165e-3


def local_points(anchor_lon_lat, geographic_lon_lat, height_m):
    """Use the existing shared camera/native ENU convention, origin at h=0."""
    longitude, latitude = np.asarray(anchor_lon_lat, dtype=float)
    points = np.asarray(geographic_lon_lat, dtype=float)
    if points.ndim != 2 or points.shape[1] != 2:
        raise ValueError("explicit N by 2 longitude/latitude coordinates required")
    return (
        ecef(points[:, 1], points[:, 0], height_m) - ecef(latitude, longitude, 0)
    ) @ axes(latitude, longitude).T


def curvature_radii(latitude_deg):
    s = 1 - WGS84_E2 * np.sin(np.deg2rad(latitude_deg)) ** 2
    return WGS84_A / np.sqrt(s), WGS84_A * (1 - WGS84_E2) / s**1.5


def query_facet(anchor_lon_lat, triangle_enu_m, geographic_lon_lat, *, metric_height_m):
    """Query the physical plane along geodetic vertical; compute its height Jacobian.

    Grades use east/north metres on the parallel surface at metric_height_m.
    A numerical barycentric diagnostic distinguishes plane extension from triangle
    interior. It does not replace the native watertight edge/BVH query.
    """
    anchor = np.asarray(anchor_lon_lat, dtype=float)
    triangle = np.asarray(triangle_enu_m, dtype=float)
    points = np.asarray(geographic_lon_lat, dtype=float)
    if (
        anchor.shape != (2,)
        or triangle.shape != (3, 3)
        or not np.isfinite(triangle).all()
        or not np.isfinite(metric_height_m)
    ):
        raise ValueError(
            "finite frame, triangle and explicit horizontal metric height required"
        )
    sea = local_points(anchor, points, 0)
    frame = axes(anchor[1], anchor[0])
    station = np.array(
        [axes(latitude, longitude) for longitude, latitude in points]
    ).reshape(-1, 3, 3)
    up = station[:, 2] @ frame.T
    edges = triangle[1:] - triangle[0]
    normal = np.cross(edges[0], edges[1])
    length = np.linalg.norm(normal)
    if not np.isfinite(length) or length == 0:
        raise ValueError("nondegenerate representable physical triangle required")
    normal /= length
    denominator = up @ normal
    valid = (abs(denominator) > 64 * np.finfo(float).eps) & (up[:, 2] > 0)
    height = np.divide(
        (triangle[0] - sea) @ normal,
        denominator,
        out=np.full(len(points), np.nan),
        where=valid,
    )
    normal_world = frame.T @ normal
    east_projection = station[:, 0] @ normal_world
    north_projection = station[:, 1] @ normal_world
    prime, meridian = curvature_radii(points[:, 1])
    if np.any(np.minimum(prime, meridian) + metric_height_m <= 0):
        raise ValueError("positive horizontal metric radii required")
    metric = np.c_[
        (prime + height) / (prime + metric_height_m),
        (meridian + height) / (meridian + metric_height_m),
    ]
    grades = (
        -np.divide(
            np.c_[east_projection, north_projection],
            denominator[:, None],
            out=np.full((len(points), 2), np.nan),
            where=valid[:, None],
        )
        * metric
    )
    keep = np.delete(np.arange(3), int(np.argmax(abs(normal))))
    barycentric = np.full((len(points), 3), np.nan)
    if valid.any():
        hit = sea[valid] + height[valid, None] * up[valid]
        uv = np.linalg.solve(edges[:, keep].T, (hit - triangle[0])[:, keep].T).T
        barycentric[valid] = np.c_[1 - uv.sum(axis=1), uv]
    return {
        "plane_height_m": height,
        "grade_m_per_m": grades,
        "barycentric": barycentric,
        "inside_numeric": valid & (barycentric >= 0).all(axis=1),
        "valid_plane_query": valid,
        "normal_dot_geodetic_up": denominator,
        "enu_up_grade": -normal[:2] / normal[2]
        if normal[2] != 0
        else np.full(2, np.nan),
    }
