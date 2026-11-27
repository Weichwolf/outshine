"""Grounded near-field trunks with continuous root flares."""

import numpy as np
import shapely
from outshine_ref.vegetation.curve_frame import arc_frame
from outshine_ref.vegetation.tube_topology import close_rings


def root_flare(azimuth, phase, radius, oak):
    bearings = phase + np.array([0.0, 1.16, 2.72, 4.0, 5.25])
    reaches = np.array([3.3, 1.85, 2.85, 1.3, 2.2]) * (1.0 if oak else 0.75)
    reaches *= 0.8 + 0.2 * np.sin(phase + 1.71 * np.arange(len(bearings)))
    lobes = np.exp((np.cos(azimuth[:, None] - bearings) - 1.0) * 26)
    return radius * (0.24 + lobes @ reaches)


def rooted_stem(plan, domain, height, position, rotation, scale, *, oak):
    axis = plan.axis(())
    radius, tip = np.asarray(plan.radii(())) * scale
    length = axis.length * scale
    first_branch = min(
        plan.axis((i,)).parent_parameter for i in range(plan.axis_counts[1])
    )
    t = np.unique(np.r_[0, 0.006, 0.013, 0.024, 0.04, 0.065, np.linspace(0.09, 1, 17)])
    azimuth = np.linspace(0, 2 * np.pi, 40, endpoint=False)
    centre, tangent, lateral, normal = arc_frame(length, axis.bend, 0, t)
    c, s = np.cos(rotation), np.sin(rotation)
    basis = np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]]) @ axis.frame
    centre = centre @ basis.T + position
    tangent = tangent @ basis.T
    radial = (
        np.cos(azimuth)[None, :, None] * lateral[:, None, :]
        + np.sin(azimuth)[None, :, None] * normal[:, None, :]
    ) @ basis.T
    # The lean dies out below the first branch so the authored joints still meet.
    q = np.clip(t / first_branch, 0, 1)
    direction = np.array([np.cos(2.3 * rotation), np.sin(2.3 * rotation), 0])
    centre += (16 * q**2 * (1 - q) ** 2 * radius * (1.05 if oak else 0.62))[
        :, None
    ] * direction
    phase = 1.7 * rotation + axis.phase
    body = radius + (tip - radius) * t
    sections = body[:, None] * (
        1
        + 0.11 * np.cos(2 * azimuth + phase + 3 * t[:, None])
        + 0.065 * np.sin(3 * azimuth - phase + 13 * t[:, None])
    )
    flare = root_flare(azimuth, phase, radius, oak)
    above_root = t * length
    skirt = np.clip(1 - above_root / (3.5 * radius), 0, 1)
    for extent in (1.0, 0.7, 0.45, 0.2, 0.0):
        collar = centre[0] + radial[0] * (sections[0] + extent * flare)[:, None]
        if domain.covers(shapely.Polygon(collar[:, :2])):
            break
    sections += extent * skirt[:, None] ** 3 * flare
    surface = centre[:, None, :] + radial * sections[..., None]
    collar = skirt > 0
    ground = height(surface[collar, :, :2].reshape(-1, 2)).reshape(-1, len(azimuth))
    ground_at_stem = height(position[None, :2])[0]
    surface[collar, :, 2] += skirt[collar, None] * (ground - ground_at_stem)
    along = np.gradient(surface, t, axis=0)
    around = np.roll(surface, -1, axis=1) - np.roll(surface, 1, axis=1)
    normals = np.cross(along, around)
    normals /= np.linalg.norm(normals, axis=2, keepdims=True)
    return close_rings(surface, normals, centre, tangent)
