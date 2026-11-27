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
    bend = axis.bend * height
    arc_sweep = 0.5 * bend * height * np.sinc(bend / (2 * np.pi)) ** 2
    lean = height**2 * (0.025 + 0.055 * np.cos(axis.phase))
    # One change of curvature in the lower stem, with a broader sweep above it.
    lower = height**2 * (1 - height) ** 5 * 28
    upper = np.sin(np.pi * height) ** 2
    lean += lower * (0.035 if oak else 0.025) * np.sin(axis.phase + 0.7)
    sweep = upper * (0.045 if oak else 0.024)
    sweep -= lower * (0.040 if oak else 0.027) * np.cos(axis.phase - 0.6)
    sweep -= (0.68 if oak else 0.90) * arc_sweep
    drift = lean[..., None] * axis.frame[:, 0] + sweep[..., None] * axis.frame[:, 2]
    return vertices + length * drift


def root_flare(azimuth, phase, radius, height, oak, rng):
    bearings = phase + np.array([0.0, 1.5, 3.12, 4.82, 5.56])
    bearings += rng.uniform(-0.28, 0.28, len(bearings))
    reaches = rng.permutation([2.45, 1.42, 1.90, 0.85, 0.45]) * (1 if oak else 0.73)
    widths = rng.uniform(9.0, 18.0, len(bearings))
    shoulders = radius * rng.uniform(2.4, 5.0, len(bearings))
    fraction = height[:, None] / shoulders
    bend = np.minimum(fraction, 1) * rng.uniform(-0.5, 0.5, len(bearings))
    angle = azimuth[None, :, None] - (bearings + bend)[:, None, :]
    lobes = np.exp((np.cos(angle) - 1) * widths)
    decay = np.maximum(1 - fraction, 0) ** 1.8
    return radius * np.sum(decay[:, None, :] * reaches * lobes, axis=2)


def stem_knots(t, azimuth, heights, bearings, widths, radius, oak):
    """An old branch leaves a shoulder that rises into an irregular scar."""
    axial = (t[:, None, None] - heights) / widths
    angle = azimuth[None, :, None] - bearings - 0.20 * axial
    distance = axial**2 + 7.5 * (1 - np.cos(angle))
    shoulder = np.exp(-distance)
    stub = np.exp(-2.0 * ((axial - 0.40) ** 2 + 7 * (1 - np.cos(angle))))
    centre = np.exp(-6 * distance)
    relief = 0.24 * shoulder + (0.60 if oak else 0.39) * stub - 0.18 * centre
    return radius * relief.sum(axis=2)


def stem_profile(plan, scale, *, oak, grounded, seed):
    axis = plan.axis(())
    radius, tip = np.asarray(plan.radii(())) * scale
    length = axis.length * scale
    rng = np.random.default_rng(seed)
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
    knots = np.array([0.11, 0.22, 0.34]) + rng.uniform(-0.037, 0.037, 3)
    bearings = axis.phase + np.array([0, 2.40, 4.80]) + rng.uniform(-0.8, 0.8, 3)
    widths = rng.uniform(0.031, 0.044, 3)
    scar_samples = [-1.25, -0.55, 0.24, 0.7, 1.4] if grounded else [0.4]
    scar_sections = knots + widths * np.asarray(scar_samples)[:, None]
    body = np.linspace(0, 1, 43 if grounded else 10)
    t = np.unique(
        np.round(
            np.r_[
                0,
                height[height < collar_end],
                collar_end,
                body[body > collar_end],
                joints,
                scar_sections.ravel(),
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
    waist = np.sin(np.pi * t) * np.sin(4 * np.pi * t + bearings[0])
    body *= 1 + (0.11 if oak else 0.08) * waist
    twist = 0.75 * np.sin(5 * along + phase) + 1.4 * along
    sections = body[:, None] * (
        1
        + (0.17 if oak else 0.12) * np.cos(2 * angle + phase + twist)
        + 0.09 * np.sin(3 * angle - phase + 2 * twist)
        + (0.05 if oak else 0.030) * np.sin(5 * angle + bearings[1] - twist)
    )
    sections += stem_knots(t, azimuth, knots, bearings, widths, radius, oak)
    for branch, at, limb_radius in zip(branches, joints, limb_radii, strict=True):
        _, _, side, up = arc_frame(length, axis.bend, 0, at)
        growth = axis.frame.T @ branch.frame[:, 1]
        bearing = np.arctan2(up @ growth, side @ growth)
        axial = np.exp(-(((t - at) / max(4 * limb_radius / length, 0.042)) ** 2))
        around = np.exp(3 * (np.cos(azimuth - bearing) - 1))
        sections += limb_radius * axial[:, None] * (0.11 + 0.49 * around)
    flare = root_flare(azimuth, phase, radius, t * length, oak, rng)
    collar = np.maximum(1 - t * length / (ROOT_COLLAR_RADII * radius), 0) ** 2
    return t, centre, tangent, radial, sections, flare, collar


def stem_mesh(
    plan, scale, *, oak, position=None, rotation=0, domain=None, height=None, seed=None
):
    grounded = height is not None
    t, centre, tangent, radial, sections, flare, collar = stem_profile(
        plan,
        scale,
        oak=oak,
        grounded=grounded,
        seed=plan.seed if seed is None else seed,
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
    along = np.gradient(surface, t, axis=0)
    around = np.roll(surface, -1, axis=1) - np.roll(surface, 1, axis=1)
    normals = np.cross(along, around)
    normals /= np.linalg.norm(normals, axis=2, keepdims=True)
    return close_rings(surface, normals, centre, tangent)


def rooted_stem(plan, domain, height, position, rotation, scale, *, oak, seed=None):
    return stem_mesh(
        plan,
        scale,
        oak=oak,
        position=position,
        rotation=rotation,
        domain=domain,
        height=height,
        seed=seed,
    )


def template_stem(plan, *, oak):
    return stem_mesh(plan, 1, oak=oak)
