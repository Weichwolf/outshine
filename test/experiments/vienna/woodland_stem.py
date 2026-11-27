"""Tapered woodland stems with buttresses and swell at live branch unions."""

import numpy as np
import shapely
from outshine_ref.vegetation.curve_frame import arc_frame
from outshine_ref.vegetation.tube_topology import close_rings

ROOT_COLLAR_RADII = 5.2


def lean_tree(plan, vertices, *, oak, scale=1):
    """Shear each horizontal section so stems, limbs and leaves move together."""
    axis = plan.axis(())
    length = axis.length * scale
    height = np.clip(vertices[..., 2] / length, 0, 1)
    lean = height * (2 - height) * (0.045 + 0.08 * np.cos(axis.phase))
    sweep = np.sin(np.pi * height) ** 2 * (0.105 if oak else 0.060)
    # The direction of the lower stem changes as it straightens below the crown.
    basal_bend = np.sin(np.pi * height) * np.exp(-4 * height)
    lean += basal_bend * (0.16 if oak else 0.09) * np.sin(axis.phase + 0.7)
    sweep -= basal_bend * (0.22 if oak else 0.15) * np.cos(axis.phase - 0.6)
    drift = lean[..., None] * axis.frame[:, 0] + sweep[..., None] * axis.frame[:, 2]
    return vertices + length * drift


def root_flare(azimuth, phase, radius, height, oak, rng):
    bearings = phase + np.array([0.0, 1.5, 3.12, 4.82, 5.56])
    bearings += rng.uniform(-0.28, 0.28, len(bearings))
    reaches = rng.permutation([3.25, 1.42, 2.25, 0.85, 0.45]) * (1 if oak else 0.78)
    widths = rng.uniform(7.0, 16.0, len(bearings))
    shoulders = radius * rng.uniform(2.7, 5.1, len(bearings))
    bend = height[:, None] / shoulders * rng.uniform(-0.65, 0.65, len(bearings))
    angle = azimuth[None, :, None] - (bearings + bend)[:, None, :]
    lobes = np.exp((np.cos(angle) - 1) * widths)
    decay = np.maximum(1 - height[:, None] / shoulders, 0) ** 2.6
    return radius * np.sum(decay[:, None, :] * reaches * lobes, axis=2)


def stem_knots(t, azimuth, phase, radius, oak, rng):
    """Healed lower branches leave an oval boss and a recessed centre."""
    height = np.array([0.10, 0.23, 0.36]) + rng.uniform(-0.035, 0.035, 3)
    bearings = phase + np.array([0, 2.40, 4.80]) + rng.uniform(-0.55, 0.55, 3)
    axial = (t[:, None, None] - height) / rng.uniform(0.034, 0.046, 3)
    angle = azimuth[None, :, None] - bearings - 0.14 * axial
    distance = axial**2 + 8.0 * (1 - np.cos(angle))
    relief = 0.34 * np.exp(-distance) - 0.21 * np.exp(-4 * distance)
    return radius * (1 if oak else 0.68) * relief.sum(axis=2)


def stem_profile(plan, scale, *, oak, grounded):
    axis = plan.axis(())
    radius, tip = np.asarray(plan.radii(())) * scale
    length = axis.length * scale
    rng = np.random.default_rng(plan.seed)
    branches = [plan.axis(path) for path in plan.children(())]
    joints = np.array([branch.parent_parameter for branch in branches])
    limb_radii = np.array([plan.radii(branch.path)[0] for branch in branches]) * scale
    root_sections = (
        np.r_[0.15, 0.35, 0.6, np.arange(0.9, ROOT_COLLAR_RADII, 0.4)]
        if grounded
        else np.array([0.35, 0.9, 1.7, 3.2])
    )
    height = root_sections * radius / length
    collar_end = min(ROOT_COLLAR_RADII * radius / length, 1)
    body = np.linspace(0, 1, 43 if grounded else 13)
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
    azimuth = np.linspace(0, 2 * np.pi, 40 if grounded else 12, endpoint=False)
    centre, tangent, lateral, normal = arc_frame(length, axis.bend, 0, t)
    centre, tangent = centre @ axis.frame.T, tangent @ axis.frame.T
    lateral, normal = lateral @ axis.frame.T, normal @ axis.frame.T
    radial = (
        np.cos(azimuth)[None, :, None] * lateral[:, None, :]
        + np.sin(azimuth)[None, :, None] * normal[:, None, :]
    )
    taper = np.expm1(0.58 * np.log1p(24 * t)) / np.expm1(0.58 * np.log(25))
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
        + (0.18 if oak else 0.12) * np.cos(2 * angle + phase + 4 * along)
        + 0.11 * np.sin(3 * angle - phase + 11 * along)
        + (0.070 if oak else 0.043) * np.sin(5 * angle + phase - 16 * along)
    )
    sections += stem_knots(t, azimuth, phase, radius, oak, rng)
    for branch, at, limb_radius in zip(branches, joints, limb_radii, strict=True):
        _, _, side, up = arc_frame(length, axis.bend, 0, at)
        growth = axis.frame.T @ branch.frame[:, 1]
        bearing = np.arctan2(up @ growth, side @ growth)
        axial = np.exp(-(((t - at) / max(4 * limb_radius / length, 0.042)) ** 2))
        around = np.exp(3 * (np.cos(azimuth - bearing) - 1))
        sections += limb_radius * axial[:, None] * (0.11 + 0.49 * around)
    flare = root_flare(azimuth, phase, radius, t * length, oak, rng)
    collar = np.maximum(1 - t * length / (ROOT_COLLAR_RADII * radius), 0) ** 2
    return centre, tangent, radial, sections, flare, collar


def stem_mesh(plan, scale, *, oak, position=None, rotation=0, domain=None, height=None):
    grounded = height is not None
    centre, tangent, radial, sections, flare, collar = stem_profile(
        plan, scale, oak=oak, grounded=grounded
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
