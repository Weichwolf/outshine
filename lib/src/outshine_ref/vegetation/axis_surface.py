"""A closed tapered circular-axis tube with analytic corner normals."""

import numpy as np

from .curve_frame import arc_frame
from .tube_topology import close_rings


def validate_axis(length, bend, maximum_radius, segments, sides):
    if not all(np.isfinite(v) for v in (length, bend, maximum_radius)):
        raise ValueError("finite axis parameters required")
    if (
        min(length, maximum_radius) <= 0
        or abs(bend) > np.pi
        or abs(bend) * maximum_radius >= length
    ):
        raise ValueError("positive dimensions and regular curved tube required")
    if (
        type(segments) is not int
        or not 2 <= segments <= 1024
        or type(sides) is not int
        or not 3 <= sides <= 128
    ):
        raise ValueError("bounded integer segment counts required")


def axis_mesh(
    length, bend, base_radius, tip_radius, segments=24, sides=8, *, bark=False
):
    if (
        not np.isfinite(tip_radius)
        or tip_radius <= 0
        or not np.isfinite(base_radius)
        or base_radius <= 0
    ):
        raise ValueError("positive finite tube radii required")
    validate_axis(length, bend, max(base_radius, tip_radius), segments, sides)
    t = np.linspace(0, 1, segments + 1)
    radius = base_radius + (tip_radius - base_radius) * t
    return tube_mesh(
        length,
        bend,
        radius,
        np.full_like(t, tip_radius - base_radius),
        sides,
        bark=bark,
    )


def profiled_axis_mesh(
    length, bend, radius_scale, profile, segments=24, sides=8, *, bark=False
):
    if np.min(profile.table) <= 0:
        raise ValueError("positive complete radius profile required")
    validate_axis(length, bend, radius_scale * np.max(profile.table), segments, sides)
    t = np.linspace(0, 1, segments + 1)
    return tube_mesh(
        length,
        bend,
        radius_scale * profile.evaluate(t),
        radius_scale * profile.derivative(t),
        sides,
        bark=bark,
    )


def tube_mesh(length, bend, radius, slope, sides, *, bark=False):
    if type(bark) is not bool:
        raise ValueError("explicit boolean bark-field request required")
    segments = len(radius) - 1
    t = np.linspace(0, 1, segments + 1)
    angle = np.arange(sides) * 2 * np.pi / sides
    centre, tangent, lateral, normal = arc_frame(length, bend, 0, t)
    radial = (
        np.cos(angle)[None, :, None] * lateral[:, None, :]
        + np.sin(angle)[None, :, None] * normal[:, None, :]
    )
    surface = centre[:, None, :] + radius[:, None, None] * radial
    axial = length - bend * radius[:, None] * np.sin(angle)[None, :]
    normals = (
        axial[:, :, None] * radial
        - np.broadcast_to(slope, t.shape)[:, None, None] * tangent[:, None, :]
    )
    normals /= np.linalg.norm(normals, axis=-1, keepdims=True)
    mesh = close_rings(surface, normals, centre, tangent)
    if not bark:
        return mesh
    q = radius[:, None] * np.cos(angle)
    d = radius[:, None] * np.sin(angle)
    s = np.broadcast_to((length * t)[:, None], q.shape)
    coordinates = np.column_stack((q.ravel(), d.ravel(), s.ravel()))
    coordinates = np.vstack((coordinates, (0, 0, 0), (0, 0, length)))
    return mesh | {
        "bark_coordinates": coordinates,
        "bark_radius": np.r_[np.repeat(radius, sides), radius[0], radius[-1]],
    }
