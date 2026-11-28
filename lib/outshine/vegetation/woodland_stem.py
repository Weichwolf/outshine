"""Tapered woodland stems with buttresses and swell at live branch unions."""

import numpy as np
import shapely

from outshine.vegetation.axis_surface import tube_mesh
from outshine.vegetation.curve_frame import arc_frame
from outshine.vegetation.tube_topology import close_rings

ROOT_COLLAR_RADII = 3.2
GROUND_BLEND_RADII = 6.0


def scaffold_mesh(length, bend, base_radius, tip_radius, segments, sides):
    """Broaden the visible branch collar while keeping its buried cap the same size."""
    t = np.linspace(0, 1, segments + 1)
    distance = t * length / base_radius
    collar = distance * np.exp(1 - distance)
    taper = t * (1.55 - 0.55 * t)
    radius = base_radius + (tip_radius - base_radius) * taper
    radius += 0.40 * base_radius * collar
    slope = (tip_radius - base_radius) * (1.55 - 1.1 * t)
    slope += 0.40 * length * (1 - distance) * np.exp(1 - distance)
    return tube_mesh(length, bend, radius, slope, sides)


def lean_tree(plan, vertices, *, oak, scale=1):
    """Shear each horizontal section so stems, limbs and leaves move together."""
    axis = plan.axis(())
    length = axis.length * scale
    height = np.clip(vertices[..., 2] / length, 0, 1)
    bend = axis.bend * height
    arc_sweep = 0.5 * bend * height * np.sinc(bend / (2 * np.pi)) ** 2
    lean = height**1.4 * (0.047 + 0.081 * np.cos(axis.phase))
    lower = (1.67 if oak else 1.35) * axis.bend * height * (1 - height) ** 2
    upper = np.sin(np.pi * height) ** 2
    bearing = axis.phase + 0.7 + 3.8 * height
    lean += lower * np.sin(bearing)
    sweep = upper * (0.045 if oak else 0.024)
    sweep += lower * np.cos(bearing)
    sweep -= (0.16 if oak else 0.35) * arc_sweep
    drift = lean[..., None] * axis.frame[:, 0] + sweep[..., None] * axis.frame[:, 2]
    return vertices + length * drift


def root_flare(azimuth, phase, radius, height, oak, rng):
    count = rng.integers(3, 6)
    bearings = phase + np.linspace(0, 2 * np.pi, count, endpoint=False)
    bearings += rng.uniform(-0.48, 0.48, count)
    reaches = rng.permutation(np.geomspace(rng.uniform(1.6, 2.6), 0.32, count))
    reaches *= 1.0 if oak else 0.78
    # Filter the narrow tips to the angular resolution of the stem mesh.
    widths = np.minimum(rng.uniform(20.0, 33.0, count), (len(azimuth) / np.pi) ** 2)
    shoulders = radius * rng.uniform(0.80, ROOT_COLLAR_RADII, count)
    fraction = np.minimum(height[:, None] / shoulders, 1)
    bend = fraction**2 * rng.uniform(-0.50, 0.50, count)
    angle = azimuth[None, :, None] - (bearings + bend)[:, None, :]
    lobes = np.exp((np.cos(angle) - 1) * widths * (1 - 0.65 * fraction[:, None, :]))
    decay = (1 - fraction) ** 2 * np.exp(-2.7 * fraction)
    return radius * np.sum(decay[:, None, :] * reaches * lobes, axis=2)


def stem_knots(t, azimuth, heights, bearings, widths, radius, oak):
    """An old branch leaves a shoulder that rises into an irregular scar."""
    axial = (t[:, None, None] - heights) / widths
    angle = azimuth[None, :, None] - bearings - 0.20 * axial
    distance = axial**2 + 13 * (1 - np.cos(angle))
    shoulder = np.exp(-distance)
    stub = np.exp(-2.0 * ((axial - 0.40) ** 2 + 7 * (1 - np.cos(angle))))
    centre = np.exp(-6 * distance)
    relief = 0.14 * shoulder + (0.34 if oak else 0.14) * stub - 0.14 * centre
    return radius * relief.sum(axis=2)


def bole_sections(t, azimuth, body, phase, oak, rng):
    # Sample swell and eccentric growth at unequal heights around the bole.
    heights = np.array([0.04, 0.20, 0.39, 0.67, 0.86]) + rng.uniform(-0.035, 0.035, 5)
    width = rng.uniform(0.079, 0.15, 5)
    bulges = np.exp(-(((t[:, None] - heights) / width) ** 2))
    girth = 1 + bulges @ rng.uniform(-0.14, 0.12, 5) * np.sin(np.pi * t)
    bearing = phase + rng.uniform(-np.pi, np.pi, 5)
    eccentricity = 0.22 if oak else 0.13
    shape = eccentricity * np.cos(2 * azimuth[:, None] - bearing)
    shape += (0.14 if oak else 0.065) * np.sin(3 * azimuth[:, None] + bearing)
    cross_section = bulges @ shape.T
    flute = np.cos(azimuth[None, :] * 7 + phase + t[:, None] * 7.4)
    cross_section += (0.046 if oak else 0.018) * flute * (1 - t[:, None])
    return (body * girth)[:, None] * (1 + cross_section)


def stem_profile(plan, scale, *, oak, grounded, seed):
    axis = plan.axis(())
    radius, tip = np.asarray(plan.radii(())) * scale
    length = axis.length * scale
    rng = np.random.default_rng(seed)
    branches = [plan.axis(path) for path in plan.children(())]
    joints = np.array([branch.parent_parameter for branch in branches])
    limb_radii = np.array([plan.radii(branch.path)[0] for branch in branches]) * scale
    root_sections = (
        np.r_[0.08, 0.18, 0.32, 0.50, np.arange(0.75, ROOT_COLLAR_RADII, 0.35)]
        if grounded
        else np.array([0.35, 0.9, 1.7, 3.2])
    )
    height = root_sections * radius / length
    collar_end = min(ROOT_COLLAR_RADII * radius / length, 1)
    knots = np.array([0.11, 0.22, 0.34]) + rng.uniform(-0.037, 0.037, 3)
    bearings = axis.phase + np.array([0, 2.40, 4.80]) + rng.uniform(-0.8, 0.8, 3)
    widths = (radius / length) * rng.uniform(0.75, 1.25, 3)
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
    azimuth = np.linspace(0, 2 * np.pi, 48 if grounded else 12, endpoint=False)
    centre, tangent, lateral, normal = arc_frame(length, axis.bend, 0, t)
    centre, tangent = centre @ axis.frame.T, tangent @ axis.frame.T
    lateral, normal = lateral @ axis.frame.T, normal @ axis.frame.T
    radial = (
        np.cos(azimuth)[None, :, None] * lateral[:, None, :]
        + np.sin(azimuth)[None, :, None] * normal[:, None, :]
    )
    taper = np.expm1(0.40 * np.log1p(44 * t)) / np.expm1(0.40 * np.log(45))
    body = radius + (tip - radius) * taper
    if len(joints):
        # Distribute part of the trunk section to the scaffolds at their takeoff heights.
        transfer = np.clip(0.5 + (t[:, None] - joints) / 0.14, 0, 1)
        area = limb_radii**2
        remaining = (1 - transfer**2 * (3 - 2 * transfer)) @ area / area.sum()
        body = np.sqrt(
            0.72 * body**2 + 0.28 * (tip**2 + (radius**2 - tip**2) * remaining)
        )
    sections = bole_sections(t, azimuth, body, axis.phase, oak, rng)
    sections += stem_knots(t, azimuth, knots, bearings, widths, radius, oak)
    for branch, at, limb_radius in zip(branches, joints, limb_radii, strict=True):
        _, _, side, up = arc_frame(length, axis.bend, 0, at)
        growth = axis.frame.T @ branch.frame[:, 1]
        bearing = np.arctan2(up @ growth, side @ growth)
        rise = (t - at) * length / max(limb_radius, 0.015 * length)
        shoulder = (rise + 0.65) / 3.3
        axial = np.exp(-(shoulder**2))
        around = np.exp(2.6 * (np.cos(azimuth - bearing) - 1))
        sections += limb_radius * axial[:, None] * (0.04 + 0.61 * around)
        joint = np.searchsorted(t, np.round(at, 10))
        support = np.maximum(1.04 * limb_radius - sections[joint], 0)
        sections += np.exp(-((rise / 2.8) ** 2))[:, None] * support
    flare = root_flare(azimuth, axis.phase, radius, t * length, oak, rng)
    return t, centre, tangent, radial, sections, flare


def stem_mesh(
    plan, scale, *, oak, position=None, rotation=0, domain=None, height=None, seed=None
):
    grounded = height is not None
    t, centre, tangent, radial, sections, flare = stem_profile(
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
        blend_height = GROUND_BLEND_RADII * plan.radii(())[0] * scale
        relative_z = surface[..., 2] - position[2]
        active = relative_z < blend_height
        fraction = np.clip(relative_z[active] / blend_height, 0, 1)
        collar = 1 - fraction**2 * (3 - 2 * fraction)
        heights = height(surface[active, :2])
        base = height(position[None, :2])[0]
        surface[active, 2] += collar * (heights - base)
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
