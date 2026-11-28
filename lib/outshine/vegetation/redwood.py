"""Coast redwood stand sites, tapered boles and layered evergreen crowns."""

from functools import cache

import numpy as np
import shapely

from outshine.vegetation.axis_surface import axis_mesh
from outshine.vegetation.curve_frame import arc_frame
from outshine.vegetation.plant_sites import patch_density, scatter_mesh, tile_sites
from outshine.vegetation.tube_topology import close_rings
from outshine.vegetation.woodland import woodland_surface
from outshine.vegetation.woodland_sites import spaced_sites
from outshine.world.mesh import Meshes

OLD_GROWTH_REDWOOD = "REDW-OG:Redwood (SESE)"
REDWOOD_TAXON = "outshine:taxon:wcvp:378988"
REDWOOD_PROFILE = "coast redwood"
REDWOOD_BARK = "redwood bark"
REDWOOD_FOLIAGE = "redwood foliage"
HEIGHT = np.array((40.0, 57.0, 77.0, 92.0))
RADIUS = np.array((0.78, 1.34, 1.94, 2.60))
CROWN_BASE = (0.34, 0.41, 0.45, 0.50)
CROWN_REACH = (4.1, 6.0, 8.4, 10.2)
STEM_SPACING = 10.4
MAX_RADIUS_MULTIPLIER = 2.10


def redwood_mask(atlas, source, xy):
    lon, lat = (np.array([source.lon, source.lat]) + xy / source.metres).T
    indices, _ = atlas.observed_cells(lat, lon)
    return atlas.observation_mask(indices, (OLD_GROWTH_REDWOOD,))


def redwood_forms(marks):
    return np.searchsorted((0.17, 0.49, 0.83), marks[:, 0])


def redwood_sites(source, domain, reach, seed, evidence, receipts):
    empty = np.empty((0, 2)), np.empty((0, 3))
    if domain is None or domain.is_empty:
        return empty
    candidates = list(
        tile_sites(
            source,
            np.zeros(2),
            np.sqrt(2) * (reach + STEM_SPACING),
            seed ^ 0x18B6,
            density=0.0072,
            cell_size=16,
            receipts=receipts,
        )
    )
    if not candidates:
        return empty
    xy, marks = (np.concatenate(rows) for rows in zip(*candidates, strict=True))
    valid = np.all(np.abs(xy) < reach + STEM_SPACING, axis=1)
    valid &= shapely.contains_xy(domain, *xy.T)
    xy, marks = xy[valid], marks[valid]
    valid = evidence(xy)
    xy, marks = xy[valid], marks[valid]
    marks[:, 2] = 0.87 + 0.25 * marks[:, 2]
    footprint = MAX_RADIUS_MULTIPLIER * RADIUS[redwood_forms(marks)] * marks[:, 2]
    valid = shapely.distance(shapely.points(xy), domain.boundary) > footprint + 0.12
    xy, marks = xy[valid], marks[valid]
    rank = patch_density(source, xy, seed ^ 0x0F62, scale=0.17)
    keep = spaced_sites(xy, rank, STEM_SPACING)
    keep &= np.all(np.abs(xy) < reach, axis=1)
    return xy[keep], marks[keep]


def trunk_centre(form, t):
    t = np.asarray(t)
    east = HEIGHT[form] * 0.013 * t**2 * np.sin(form + 3.4 * t)
    north = HEIGHT[form] * 0.018 * t**2 * np.cos(form + 2.1 * t)
    return np.stack((east, north, HEIGHT[form] * t), axis=-1)


@cache
def redwood_stem(form, detailed):
    radius = RADIUS[form]
    t = np.unique(
        np.r_[
            0,
            0.005,
            0.011,
            0.019,
            0.030,
            0.048,
            0.073,
            np.linspace(0.10, 1, 29 if detailed else 10),
        ]
    )
    angle = -np.linspace(0, 2 * np.pi, 48 if detailed else 12, endpoint=False)
    radial = np.column_stack((np.cos(angle), np.sin(angle), np.zeros(len(angle))))
    centre = trunk_centre(form, t)
    girth = np.interp(
        t,
        (0, 0.022, 0.085, 0.28, 0.56, 0.83, 1),
        (1.28, 1.02, 0.91, 0.75, 0.52, 0.27, 0.016),
    )
    rng = np.random.default_rng(1177 + 17 * form)
    bearings = np.linspace(0, 2 * np.pi, 5, endpoint=False) + rng.uniform(
        -0.40, 0.40, 5
    )
    reach = rng.uniform(0.12, 0.71, 5)
    lobes = np.exp(13 * (np.cos(angle[:, None] - bearings) - 1)) @ reach
    flare = np.exp(-t * HEIGHT[form] / (1.35 * radius))
    flute = 0.047 * np.cos(5 * angle + form + t[:, None] * 2) + 0.030 * np.sin(
        8 * angle - t[:, None] * 5
    )
    section = radius * (girth[:, None] * (1 + flute) + flare[:, None] * lobes)
    surface = centre[:, None, :] + section[..., None] * radial
    along = np.gradient(surface, t, axis=0)
    around = np.roll(surface, -1, axis=1) - np.roll(surface, 1, axis=1)
    normals = np.cross(along, around)
    normals /= np.linalg.norm(normals, axis=2, keepdims=True)
    tangent = np.gradient(centre, t, axis=0)
    tangent /= np.linalg.norm(tangent, axis=1, keepdims=True)
    part = close_rings(surface, normals, centre, tangent)
    return part["vertices"], part["faces"]


def foliage_spray(origin, side, forward, length, width, lift):
    rise = np.array([0.0, 0.0, lift])
    vertices = origin + np.array(
        [
            -0.70 * length * forward,
            -0.22 * length * forward - width * side,
            0.42 * length * forward - 0.61 * width * side + 0.45 * rise,
            0.69 * length * forward + rise,
            0.28 * length * forward + 0.76 * width * side + 0.42 * rise,
            -0.30 * length * forward + 0.87 * width * side,
            0.47 * rise,
            -0.42 * rise,
        ]
    )
    edge = np.arange(6)
    faces = np.vstack(
        (
            np.c_[np.full(6, 6), (edge + 1) % 6, edge],
            np.c_[np.full(6, 7), edge, (edge + 1) % 6],
        )
    )
    return vertices, faces


@cache
def redwood_geometry(form, detailed, has_stem):
    meshes = Meshes()
    if has_stem:
        meshes.add(REDWOOD_BARK, *redwood_stem(form, False))
    rng = np.random.default_rng(2063 + 23 * form)
    count = 27
    base = CROWN_BASE[form]
    fractions = np.linspace(base, 0.995, count) + rng.uniform(-0.016, 0.005, count)
    for index, t in enumerate(fractions):
        progress = (t - base) / (1 - base)
        spread = (0.58 + 0.64 * np.sin(np.pi * np.clip(progress, 0, 1)) ** 0.8) * (
            1.04 - 0.83 * progress**2
        )
        length = CROWN_REACH[form] * spread * rng.uniform(0.66, 1.22)
        heading = 2.39996 * index + rng.uniform(-0.56, 0.56)
        forward = np.array([np.cos(heading), np.sin(heading), 0.0])
        side = np.array([forward[1], -forward[0], 0.0])
        frame = np.column_stack((side, forward, (0, 0, 1)))
        start = trunk_centre(form, t)
        bend = rng.uniform(0.15, 0.74)
        branch_r = RADIUS[form] * (0.20 - 0.14 * progress) * rng.uniform(0.62, 1.1)
        if detailed:
            branch = axis_mesh(length, bend, branch_r, branch_r * 0.12, 6, 7)
            meshes.add(
                REDWOOD_BARK, branch["vertices"] @ frame.T + start, branch["faces"]
            )
        samples = np.linspace(0.33, 0.99, 10)
        centres = arc_frame(length, bend, 0, samples)[0] @ frame.T + start
        for spray, (sample, centre) in enumerate(zip(samples, centres, strict=True)):
            size = length * rng.uniform(0.16, 0.31) * (1.19 - 0.46 * sample)
            flip = rng.choice((-1, 1))
            lateral = flip * size * rng.uniform(0.4, 0.98)
            if not detailed and spray % 2:
                continue
            offset = centre + lateral * side
            size *= 1.0 if detailed else 1.17
            meshes.add(
                REDWOOD_FOLIAGE,
                *foliage_spray(offset, side, forward, size, 0.72 * size, 0.43 * size),
            )
    return meshes


def redwood_batches(soil, sites, eye, radius, *, eye_route=None):
    xy, marks = sites
    valid = np.all(np.abs(xy) < radius, axis=1)
    xy, marks = xy[valid], marks[valid]
    if not len(xy) or (sampler := woodland_surface(soil)) is None:
        return
    height = sampler.sample(xy).points_enu_m[:, 2]
    valid = np.isfinite(height)
    xy, marks, height = xy[valid], marks[valid], height[valid]
    forms = redwood_forms(marks)
    anchors = np.column_stack((xy, height - 0.12))
    angles = marks[:, 1] * 2 * np.pi
    scales = marks[:, 2]
    distance = (
        shapely.distance(shapely.points(xy), eye_route)
        if eye_route is not None
        else np.linalg.norm(xy - eye, axis=1)
    )
    rooted = Meshes()
    placements = np.column_stack((anchors, angles, scales))
    for form in range(len(HEIGHT)):
        nearby = (forms == form) & (distance < 36)
        if nearby.any():
            mesh = redwood_stem(form, True)
            for index in np.flatnonzero(nearby):
                anchor = anchors[index]
                scale = scales[index]
                points, faces = scatter_mesh(
                    mesh,
                    anchor[None],
                    angles[index : index + 1],
                    scales[index : index + 1],
                )
                rise = (points[:, 2] - anchor[2]) / (3 * RADIUS[form] * scale)
                active = rise < 1
                weight = np.clip(1 - rise[active], 0, 1)
                ground = sampler.sample(points[active, :2]).points_enu_m[:, 2]
                delta = np.where(np.isfinite(ground), ground - height[index], 0)
                points[active, 2] += delta * weight**2 * (3 - 2 * weight)
                rooted.add(REDWOOD_BARK, points, faces)
        for detailed, has_stem, selected in (
            (True, False, nearby),
            (True, True, (forms == form) & (distance >= 36) & (distance < 115)),
            (False, True, (forms == form) & (distance >= 115)),
        ):
            if selected.any():
                yield (
                    REDWOOD_PROFILE,
                    form,
                    redwood_geometry(form, detailed, has_stem),
                    placements[selected],
                )
    if rooted:
        yield "rooted redwoods", None, rooted, np.array([[0, 0, 0, 0, 1]])
