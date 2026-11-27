"""Tapered woodland stems with buttresses and swell at live branch unions."""

import numpy as np
import shapely
from outshine_ref.vegetation.curve_frame import arc_frame
from outshine_ref.vegetation.tube_topology import close_rings


def lean_tree(plan, vertices, *, oak, scale=1):
    """Shear each horizontal section so stems, limbs and leaves move together."""
    axis = plan.axis(())
    length = axis.length * scale
    height = np.clip(vertices[..., 2] / length, 0, 1)
    lean = height * (2 - height) * (0.075 + 0.045 * np.cos(axis.phase))
    sweep = np.sin(np.pi * height) ** 2 * (0.105 if oak else 0.075)
    drift = lean[..., None] * axis.frame[:, 0] + sweep[..., None] * axis.frame[:, 2]
    return vertices + length * drift


def root_flare(azimuth, phase, radius, height, oak):
    bearings = phase + np.array([0.0, 1.5, 3.12, 4.82, 5.56])
    reaches = np.array([3.10, 1.22, 2.05, 0.92, 0.49]) * (1 if oak else 0.83)
    widths = np.array([13.0, 15.0, 12.0, 17.0, 19.0])
    shoulders = radius * np.array([2.9, 3.2, 2.5, 1.7, 2.15])
    lobes = np.exp((np.cos(azimuth[:, None] - bearings) - 1) * widths)
    decay = np.maximum(1 - height[:, None] / shoulders, 0) ** 3
    return radius * ((decay * reaches) @ lobes.T)


def stem_profile(plan, scale, *, oak, sides, spacing):
    axis = plan.axis(())
    radius, tip = np.asarray(plan.radii(())) * scale
    length = axis.length * scale
    branches = [plan.axis(path) for path in plan.children(())]
    joints = np.array([branch.parent_parameter for branch in branches])
    limb_radii = np.array([plan.radii(branch.path)[0] for branch in branches]) * scale
    height = np.r_[0.15, 0.35, 0.6, np.arange(0.9, 3.7, 0.4)] * radius / length
    collar_end = min(3.7 * radius / length, 1)
    body = np.linspace(0, 1, spacing)
    t = np.unique(
        np.round(
            np.r_[
                0,
                height[height < collar_end],
                collar_end,
                body[body > collar_end],
                joints,
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
    taper = np.expm1(0.67 * np.log1p(36 * t)) / np.expm1(0.67 * np.log(37))
    body = radius + (tip - radius) * taper
    if len(joints):
        # Distribute part of the trunk section to the scaffolds at their takeoff heights.
        transfer = np.clip(0.5 + (t[:, None] - joints) / 0.14, 0, 1)
        area = limb_radii**2
        remaining = (1 - transfer**2 * (3 - 2 * transfer)) @ area / area.sum()
        body = np.sqrt(
            0.56 * body**2 + 0.44 * (tip**2 + (radius**2 - tip**2) * remaining)
        )
    phase = axis.phase
    angle = azimuth[None, :]
    along = t[:, None]
    sections = body[:, None] * (
        1
        + (0.155 if oak else 0.105) * np.cos(2 * angle + phase + 3 * along)
        + 0.090 * np.sin(3 * angle - phase + 9 * along)
        + 0.042 * np.sin(5 * angle + phase - 13 * along)
    )
    for branch, at, limb_radius in zip(branches, joints, limb_radii, strict=True):
        _, _, side, up = arc_frame(length, axis.bend, 0, at)
        growth = axis.frame.T @ branch.frame[:, 1]
        bearing = np.arctan2(up @ growth, side @ growth)
        axial = np.exp(-(((t - at) / max(4 * limb_radius / length, 0.042)) ** 2))
        around = np.exp(3 * (np.cos(azimuth - bearing) - 1))
        sections += 0.53 * limb_radius * axial[:, None] * around
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
        spacing=28 if grounded else 14,
    )
    surface = centre[:, None, :] + radial * sections[..., None]
    surface = lean_tree(plan, surface, oak=oak, scale=scale)
    centre = lean_tree(plan, centre, oak=oak, scale=scale)
    if grounded:
        c, s = np.cos(rotation), np.sin(rotation)
        frame = np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])
        centre = centre @ frame.T + position
        tangent = tangent @ frame.T
        radial = radial @ frame.T
        surface = surface @ frame.T + position
        for extent in (1.0, 0.75, 0.5, 0.25, 0.0):
            foot = surface[0] + radial[0] * (extent * flare[0])[:, None]
            if domain.covers(shapely.Polygon(foot[:, :2])):
                break
        flare *= extent
    surface += radial * flare[..., None]
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
