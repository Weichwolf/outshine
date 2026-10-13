"""Profiled elliptical sweep surfaces along regular authored spatial branch spines."""

import numpy as np
from spine_frames import transport_frames
from tube_topology import close_rings


def validate_surface(spine, profile, radius_scale, ellipse, segments, sides, roll):
    ellipse = np.asarray(ellipse, dtype=np.float64)
    roll = np.asarray(roll, dtype=np.float64)
    if (
        ellipse.shape != (2,)
        or roll.shape != (2,)
        or not np.isfinite(ellipse).all()
        or not np.isfinite(roll).all()
        or not np.isfinite(radius_scale)
        or radius_scale <= 0
        or np.min(ellipse) <= 0
        or np.min(profile.table) <= 0
    ):
        raise ValueError(
            "positive radii/elliptical axes and finite endpoint roll required"
        )
    if not isinstance(segments, int) or not 2 <= segments <= 1024:
        raise ValueError("two through 1024 axial segments required")
    if not isinstance(sides, int) or not 3 <= sides <= 128:
        raise ValueError("three through 128 circumferential sides required")
    radius = radius_scale * np.max(profile.table) * np.max(ellipse)
    if not np.isfinite(radius) or radius * spine.curvature_bound >= 1:
        raise ValueError(
            "radius must stay below the conservative local curvature limit"
        )
    return ellipse, roll


def sweep_stations(
    centre, tangent, speed, curvature, frames, radius, slope, ellipse, roll_rate, angle
):
    lateral, normal = frames[:, 0], frames[:, 2]
    cosine = ellipse[0] * np.cos(angle)[None, :, None]
    sine = ellipse[1] * np.sin(angle)[None, :, None]
    offset = cosine * lateral[:, None] + sine * normal[:, None]
    angular = (
        -ellipse[0] * np.sin(angle)[None, :, None] * lateral[:, None]
        + ellipse[1] * np.cos(angle)[None, :, None] * normal[:, None]
    )
    axial_factor = 1 - radius[:, None] * np.sum(curvature[:, None] * offset, axis=-1)
    longitudinal = (
        (speed[:, None] * axial_factor)[..., None] * tangent[:, None]
        + slope[:, None, None] * offset
        + (radius * roll_rate)[:, None, None]
        * (cosine * normal[:, None] - sine * lateral[:, None])
    )
    surface = centre[:, None] + radius[:, None, None] * offset
    normals = np.cross(longitudinal, radius[:, None, None] * angular)
    normals /= np.linalg.norm(normals, axis=-1, keepdims=True)
    if not np.isfinite(surface).all() or not np.isfinite(normals).all():
        raise ValueError("finite regular sweep geometry required")
    return surface, normals


def spine_mesh(
    spine,
    profile,
    radius_scale,
    segments=64,
    sides=16,
    ellipse=(1, 1),
    roll=(0, 0),
    initial=(1, 0, 0),
):
    ellipse, roll = validate_surface(
        spine, profile, radius_scale, ellipse, segments, sides, roll
    )
    parameter = np.linspace(0, 1, segments + 1)
    centre, tangent, speed, curvature = spine.sample(parameter)
    frames = transport_frames(
        centre, tangent, initial, np.interp(parameter, (0, 1), roll)
    )
    surface, normals = sweep_stations(
        centre,
        tangent,
        speed,
        curvature,
        frames,
        radius_scale * profile.evaluate(parameter),
        radius_scale * profile.derivative(parameter),
        ellipse,
        roll[1] - roll[0],
        np.arange(sides) * 2 * np.pi / sides,
    )
    return close_rings(surface, normals, centre, tangent)
