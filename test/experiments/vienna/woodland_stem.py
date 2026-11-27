"""Tapered woodland stems with buttresses and swell at live branch unions."""

import numpy as np
import shapely
from outshine_ref.vegetation.curve_frame import arc_frame
from outshine_ref.vegetation.tube_topology import close_rings


def root_flare(azimuth, phase, radius, height, oak):
    bearings = phase + np.array([0.0, 1.6, 3.5, 4.65])
    reaches = np.array([1.44, 0.85, 1.16, 0.54]) * (1 if oak else 0.68)
    widths = np.array([7.0, 11.0, 8.0, 10.0])
    shoulders = radius * np.array([3.7, 2.4, 3.2, 1.8])
    lobes = np.exp((np.cos(azimuth[:, None] - bearings) - 1) * widths)
    decay = np.maximum(1 - height[:, None] / shoulders, 0) ** 2
    return radius * ((decay * reaches) @ lobes.T)


def stem_profile(plan, scale, *, oak, sides, spacing):
    axis = plan.axis(())
    radius, tip = np.asarray(plan.radii(())) * scale
    length = axis.length * scale
    branches = [plan.axis(path) for path in plan.children(())]
    first = branches[0].parent_parameter if branches else 1.0
    height = np.arange(0.5, 4.5, 0.5) * radius / length
    collar_end = min(3.7 * radius / length, 1)
    body = np.linspace(0, 1, spacing)
    t = np.unique(
        np.round(
            np.r_[
                0,
                height[height < collar_end],
                collar_end,
                body[body > collar_end],
                first,
            ],
            10,
        )
    )
    azimuth = np.linspace(0, 2 * np.pi, sides, endpoint=False)
    centre, tangent, lateral, normal = arc_frame(length, axis.bend, 0, t)
    centre, tangent = centre @ axis.frame.T, tangent @ axis.frame.T
    lateral, normal = lateral @ axis.frame.T, normal @ axis.frame.T
    radial = (
        np.cos(azimuth)[None, :, None] * lateral[:, None, :]
        + np.sin(azimuth)[None, :, None] * normal[:, None, :]
    )
    phase = axis.phase
    # Both displacement and slope return to the authored axis at the first union.
    q = np.clip(t / max(first, 1e-10), 0, 1)
    direction = np.array([np.cos(phase * 2), np.sin(phase * 2), 0])
    centre += (16 * q**2 * (1 - q) ** 2 * radius * (1.30 if oak else 0.77))[
        :, None
    ] * direction

    taper = np.expm1(0.67 * np.log1p(36 * t)) / np.expm1(0.67 * np.log(37))
    body = radius + (tip - radius) * taper
    angle = azimuth[None, :]
    along = t[:, None]
    sections = body[:, None] * (
        1
        + 0.125 * np.cos(2 * angle + phase + 3 * along)
        + 0.075 * np.sin(3 * angle - phase + 10 * along)
        + 0.048 * np.sin(5 * angle + phase - 18 * along)
    )
    for branch in branches:
        at = branch.parent_parameter
        _, _, side, up = arc_frame(length, axis.bend, 0, at)
        growth = axis.frame.T @ branch.frame[:, 1]
        bearing = np.arctan2(up @ growth, side @ growth)
        limb_radius = plan.radii(branch.path)[0] * scale
        axial = np.exp(-(((t - at) / max(2.7 * limb_radius / length, 0.034)) ** 2))
        around = np.exp(4 * (np.cos(azimuth - bearing) - 1))
        sections += 0.58 * limb_radius * axial[:, None] * around
    flare = root_flare(azimuth, phase, radius, t * length, oak)
    collar = np.maximum(1 - t * length / (3.7 * radius), 0) ** 2
    return centre, tangent, radial, sections, flare, collar


def stem_mesh(plan, scale, *, oak, position=None, rotation=0, domain=None, height=None):
    grounded = height is not None
    centre, tangent, radial, sections, flare, collar = stem_profile(
        plan,
        scale,
        oak=oak,
        sides=36 if grounded else 12,
        spacing=33 if grounded else 17,
    )
    if grounded:
        c, s = np.cos(rotation), np.sin(rotation)
        frame = np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])
        centre = centre @ frame.T + position
        tangent = tangent @ frame.T
        radial = radial @ frame.T
        for extent in (1.0, 0.75, 0.5, 0.25, 0.0):
            foot = centre[0] + radial[0] * (sections[0] + extent * flare[0])[:, None]
            if domain.covers(shapely.Polygon(foot[:, :2])):
                break
        flare *= extent
    surface = centre[:, None, :] + radial * (sections + flare)[..., None]
    if grounded:
        active = collar > 0
        xy = surface[active, :, :2]
        heights = height(xy.reshape(-1, 2)).reshape(xy.shape[:2])
        base = height(position[None, :2])[0]
        surface[active, :, 2] += collar[active, None] * (heights - base)
    along = np.gradient(surface, axis=0)
    around = np.roll(surface, -1, axis=1) - np.roll(surface, 1, axis=1)
    normals = np.cross(along, around)
    normals /= np.linalg.norm(normals, axis=2, keepdims=True)
    return close_rings(surface, normals, centre, tangent)


def rooted_stem(plan, domain, height, position, rotation, scale, *, oak):
    return stem_mesh(
        plan,
        scale,
        oak=oak,
        position=position,
        rotation=rotation,
        domain=domain,
        height=height,
    )


def template_stem(plan, *, oak):
    return stem_mesh(plan, 1, oak=oak)
