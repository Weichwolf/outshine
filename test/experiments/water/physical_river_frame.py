"""Explicit WGS84 frame, height targets and whole-prism normal bounds (#760)."""

import numpy as np
from geodetic_facet import WGS84_A, WGS84_E2
from outshine_ref.places.camera import axes
from pyproj import Transformer
from pyproj.enums import TransformDirection


def inverse_frame(anchor_lon_lat, points_enu_m):
    anchor = np.asarray(anchor_lon_lat, float)
    points = np.asarray(points_enu_m, float)
    if (
        anchor.shape != (2,)
        or not np.isfinite(anchor).all()
        or abs(anchor[0]) > 180
        or abs(anchor[1]) > 90
        or points.ndim != 2
        or points.shape[1] != 3
        or not np.isfinite(points).all()
    ):
        raise ValueError("finite WGS84 anchor and N by 3 ENU points required")
    transform = Transformer.from_pipeline(
        "+proj=pipeline +step +proj=cart +ellps=WGS84 "
        f"+step +proj=topocentric +ellps=WGS84 +lon_0={anchor[0]} "
        f"+lat_0={anchor[1]} +h_0=0"
    )
    lon, lat, height = transform.transform(
        *points.T, direction=TransformDirection.INVERSE
    )
    geographic = np.c_[lon, lat]
    up = np.array([axes(y, x)[2] for x, y in geographic]).reshape(-1, 3)
    up = up @ axes(anchor[1], anchor[0]).T
    return geographic, np.asarray(height), up


def model_height_targets(anchor, xy, model_height_m):
    """Intersect fixed ENU XY lines with declared constant-geodetic-height targets.

    Changing Up at fixed XY also changes geography. These are model coordinates,
    never an observed ASL/geoid conversion. Newton uses dh/d(ENU-Up) = up.z.
    """
    xy, height = np.asarray(xy, float), np.asarray(model_height_m, float)
    if xy.ndim != 2 or xy.shape[1] != 2 or height.shape != (len(xy),):
        raise ValueError("one declared geodetic model height per fixed XY required")
    points = np.c_[xy, height]
    for _ in range(6):
        _geographic, actual, up = inverse_frame(anchor, points)
        if np.any(up[:, 2] < 0.9):
            raise ValueError("target outside the local ENU graph domain")
        points[:, 2] += (height - actual) / up[:, 2]
    _, actual, _ = inverse_frame(anchor, points)
    if np.max(abs(actual - height), initial=0) > 64 * np.spacing(WGS84_A):
        raise ValueError("geodetic target intersection did not converge")
    return points[:, 2]


def normal_caps(anchor, vertices_xy, faces, up_bounds_m):
    """Enclose every geodetic unit Up over each entire bounded physical prism.

    z>0 is outside the ellipsoid's supporting tangent plane at the anchor. Hence
    h>=z_min throughout the prism, and ||D Up|| <= 1/(a(1-e²)+z_min).
    A ball about the prism centre encloses its six extreme vertices and all of
    their convex hull. Integrating the derivative along each radius bounds Up.
    """
    low, high = np.asarray(up_bounds_m, float)
    if not np.isfinite([low, high]).all() or not 0 < low < high:
        raise ValueError("explicit positive ordered ENU-Up bounds required")
    triangle = np.asarray(vertices_xy, float)[faces]
    centre_xy = triangle.mean(axis=1)
    centre = np.c_[centre_xy, np.full(len(faces), (low + high) / 2)]
    radius = np.sqrt(
        np.max(np.sum((triangle - centre_xy[:, None]) ** 2, axis=2), axis=1)
        + ((high - low) / 2) ** 2
    )
    delta = radius / (WGS84_A * (1 - WGS84_E2) + low) + 1e-12
    if not np.isfinite(delta).all() or np.any(delta >= 0.01):
        raise ValueError("whole-prism normal cap exceeds the declared local domain")
    geographic, _height, up = inverse_frame(anchor, centre)
    return {
        "centre_enu_m": centre,
        "centre_lon_lat": geographic,
        "up": up,
        "delta": delta,
        "radius_m": radius,
        "up_bounds_m": np.array([low, high]),
    }


def tangent_directions(up, directions_enu):
    directions = np.asarray(directions_enu, float)
    if directions.shape != up.shape or not np.isfinite(directions).all():
        raise ValueError("one finite ENU direction per normal cap required")
    tangent = directions - (directions * up).sum(axis=1)[:, None] * up
    length = np.linalg.norm(tangent, axis=1)
    if not np.isfinite(length).all() or np.any(length <= 1e-12):
        raise ValueError("nonzero horizontal projection required")
    return tangent / length[:, None]


def normal_norm_bounds(caps, row_faces, tangent, intervals, primary_rows):
    """Derive |(-gx,-gy,1)| bounds from two existing direction intervals per face.

    No new slope limit is introduced. At the cap centre, two independent tangent
    grade bounds imply |grad_h|<=B. Since n.z=1 and n=(n.u)(u-grad_h),
    |n|<=sqrt(1+B²)/(u.z-B*|projection_tangent(anchor_Up)|).
    The linear prism rows themselves enforce these centre grades before the
    derived norm bound is used to justify their whole-prism interpretation.
    """
    selected = np.asarray(primary_rows)
    count = len(caps["up"])
    if (
        selected.shape != (count, 2)
        or selected.dtype.kind not in "ui"
        or np.any(selected < 0)
        or np.any(selected >= len(row_faces))
        or np.any(row_faces[selected] != np.arange(count)[:, None])
    ):
        raise ValueError(
            "two existing same-face primary row indices per facet required"
        )
    first, second = tangent[selected[:, 0]], tangent[selected[:, 1]]
    minimum_eigenvalue = 1 - abs(np.sum(first * second, axis=1)) - 1e-12
    if np.any(minimum_eigenvalue <= 0):
        raise ValueError(
            "independent primary tangents required for a finite norm bound"
        )
    maximum_grade = abs(intervals[selected]).max(axis=2)
    grade_bound = np.linalg.norm(maximum_grade, axis=1) / np.sqrt(minimum_eigenvalue)
    up = caps["up"]
    horizontal_up = (
        np.linalg.norm(np.array([0.0, 0.0, 1.0]) - up * up[:, 2, None], axis=1) + 1e-12
    )
    denominator = up[:, 2] - grade_bound * horizontal_up - 1e-12
    if np.any(denominator <= 0):
        raise ValueError("primary intervals do not bound normals in this ENU graph")
    return np.sqrt(1 + grade_bound**2) / denominator * (1 + 1e-12)
