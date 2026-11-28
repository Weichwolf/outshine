"""Generate fern and tall-grass patches on exposed woodland ground."""

from functools import cache

import numpy as np
import shapely
from scipy.spatial.transform import Rotation

from outshine.terrain.ground_surface import surface_elevations
from outshine.vegetation import Blade, FrondGuide, PinnatePattern
from outshine.vegetation.axis_surface import axis_mesh
from outshine.vegetation.graminoid import graminoid_tuft_mesh
from outshine.vegetation.plant_sites import patch_density, scatter_mesh, tile_sites

UNDERSTORY_REACH_M = 35.0
LOW_COVER_REACH_M = 20.0
LOW_COVER_MAX_SCALE = 1.16


@cache
def fern():
    return pinnate_fern(7, 0.64, -1.13, 11, 5, 0)


@cache
def sword_fern(form, detailed):
    return pinnate_fern(
        (10 if detailed else 6) + form,
        0.83 + 0.10 * form,
        -1.26,
        18 if detailed else 6,
        4 if detailed else 2,
        form + 1,
    )


def pinnate_fern(fronds, size, bend, pairs, samples, variant):
    vertices, faces = [], []
    start = 0
    for i in range(fronds):
        length = size + 0.11 * np.sin(3 * i + 0.7 + variant)
        pattern = PinnatePattern(
            seed=31 + i + variant * 97,
            length=length,
            pairs=pairs,
            leaflet_length=0.24 if not variant else 0.18,
            aspect=0.29,
        )
        guide_length = np.nextafter(length, np.inf) if variant else length
        frond = FrondGuide(length=guide_length, bend=bend, fold_per_m=1.2).deform(
            pattern.mesh(samples, 1)
        )
        rachis = axis_mesh(length, bend, 0.0033, 0.0007, max(samples + 2, 4), 5)
        heading, lean = i * 2.4, 0.95 + 0.23 * np.sin(i * 4.1)
        rotation = Rotation.from_euler("ZX", (heading, lean))
        for part in (frond, rachis):
            vertices.append(rotation.apply(part["vertices"]))
            faces.append(part["faces"] + start)
            start += len(part["vertices"])
    return np.concatenate(vertices), np.concatenate(faces)


@cache
def tall_grass():
    mesh = graminoid_tuft_mesh(55, blades=19, segments=4)
    return mesh["vertices"], mesh["faces"]


@cache
def low_cover_tuft():
    stem = axis_mesh(0.062, 0, 0.0036, 0.0014, 3, 4)
    vertices = [stem["vertices"][:, [0, 2, 1]] * [1, -1, 1]]
    faces = [stem["faces"]]
    for i, (length, height, tilt) in enumerate(
        (
            (0.18, 0.04, 0.29),
            (0.14, 0.03, 0.21),
            (0.17, 0.03, 0.39),
            (0.12, 0.05, 0.28),
            (0.14, 0.06, 0.48),
        )
    ):
        leaf = Blade(
            length=length,
            width=length * 0.64,
            widest=0.37,
            lobes=5,
            lobe_depth=0.15,
            base_fill=0.42,
            bend=0.18,
            fold=0.11,
            petiole=0.014,
        ).mesh(3)
        rotation = Rotation.from_euler("ZX", (i * 2.4, tilt))
        faces.append(leaf["faces"] + sum(map(len, vertices)))
        vertices.append(rotation.apply(leaf["vertices"]) + [0, 0, height])
    return np.concatenate(vertices), np.concatenate(faces)


def woodland_low_cover(mesh, source, domain, eye, seed, *, exclude=None):
    centre = eye[:2]
    template = low_cover_tuft()
    reach = np.linalg.norm(template[0][:, :2], axis=1).max() * LOW_COVER_MAX_SCALE
    # Reserve the template's whole footprint along paths and around water.
    area = domain.intersection(
        shapely.Point(centre).buffer(LOW_COVER_REACH_M + reach)
    ).buffer(-reach)
    if area.is_empty:
        return
    sites = list(
        tile_sites(
            source, centre, LOW_COVER_REACH_M, seed ^ 0x42CF, density=12, cell_size=4
        )
    )
    if not sites:
        return
    xy, marks = (np.concatenate(col) for col in zip(*sites, strict=True))
    nearby = shapely.contains_xy(area, *xy.T)
    if exclude is not None:
        nearby &= ~exclude(xy)
    xy, marks = xy[nearby], marks[nearby]
    patches = patch_density(source, xy, seed ^ 0x7D71, scale=0.55)
    fade = np.clip((LOW_COVER_REACH_M - np.linalg.norm(xy - centre, axis=1)) / 7, 0, 1)
    planted = marks[:, 2] < fade * np.clip((patches - 0.32) * 1.55, 0, 0.86)
    if not planted.any():
        return
    xy, marks = xy[planted], marks[planted]
    heights = surface_elevations(mesh, xy)
    grounded = np.isfinite(heights)
    if grounded.any():
        anchors = np.column_stack((xy[grounded], heights[grounded] - 0.009))
        marks = marks[grounded]
        sizes = 0.64 + (LOW_COVER_MAX_SCALE - 0.64) * marks[:, 1]
        yield (
            "woodland low leaves",
            *scatter_mesh(template, anchors, marks[:, 0] * (2 * np.pi), sizes),
        )


def woodland_cover(mesh, source, domain, eye, seed, *, exclude=None):
    if domain is None:
        return
    yield from woodland_low_cover(mesh, source, domain, eye, seed, exclude=exclude)
    centre = eye[:2]
    window = shapely.Point(centre).buffer(UNDERSTORY_REACH_M)
    planting = domain.intersection(window.buffer(1)).buffer(-0.72)
    if planting.is_empty:
        return
    sites = list(
        tile_sites(
            source, centre, UNDERSTORY_REACH_M, seed ^ 763981, density=1.35, cell_size=4
        )
    )
    if not sites:
        return
    xy = np.concatenate([b[0] for b in sites])
    marks = np.concatenate([b[1] for b in sites])
    inside = shapely.contains_xy(planting, *xy.T)
    if exclude is not None:
        inside &= ~exclude(xy)
    xy, marks = xy[inside], marks[inside]
    heights = surface_elevations(mesh, xy)
    grounded = np.isfinite(heights)
    xy, marks = xy[grounded], marks[grounded]
    anchors = np.column_stack((xy, heights[grounded] - 0.018))

    lon, lat = (np.array([source.lon, source.lat]) + xy / source.metres).T
    patches = np.sin(28200 * lon + 23300 * lat) * np.sin(12600 * lon - 52200 * lat)
    fern_cover = np.maximum(patches, 0)
    reach = np.linalg.norm(anchors - eye, axis=1)
    coverage = np.clip((UNDERSTORY_REACH_M - reach) / 12, 0, 1)
    scales = 0.62 + 0.56 * marks[:, 1]
    angles = 2 * np.pi * marks[:, 0]
    for name, group, plant in (
        ("woodland ferns", marks[:, 2] < coverage * (0.07 + 0.57 * fern_cover), fern),
        (
            "woodland grasses",
            1 - marks[:, 2] < coverage * (0.55 - 0.32 * fern_cover),
            tall_grass,
        ),
    ):
        if group.any():
            yield (
                name,
                *scatter_mesh(plant(), anchors[group], angles[group], scales[group]),
            )


def redwood_ferns(mesh, source, domain, eye, seed, evidence):
    if domain is None or domain.is_empty:
        return
    reach = 76
    area = domain.intersection(shapely.Point(eye[:2]).buffer(reach + 1.25)).buffer(
        -1.25
    )
    sites = list(
        tile_sites(source, eye[:2], reach, seed ^ 0xCEA8, density=2.5, cell_size=5)
    )
    if not sites:
        return
    xy, marks = (np.concatenate(rows) for rows in zip(*sites, strict=True))
    selected = shapely.contains_xy(area, *xy.T) & evidence(xy)
    xy, marks = xy[selected], marks[selected]
    distance = np.linalg.norm(xy - eye[:2], axis=1)
    mix = patch_density(source, xy, seed ^ 0x77AF, scale=1.8)
    nearby = np.clip(1 - (distance - 19) / 37, 0, 1)
    probability = (0.22 + 0.71 * mix) * (0.26 + 0.74 * nearby**1.4)
    probability *= np.clip((reach - distance) / 14, 0, 1)
    selected = marks[:, 2] < probability
    xy, marks, distance = xy[selected], marks[selected], distance[selected]
    height = surface_elevations(mesh, xy)
    anchors = np.column_stack((xy, height - 0.025))
    forms = marks[:, 1] > 0.46
    detail = distance < 20
    scale = 0.71 + 0.55 * np.mod(17 * marks[:, 1], 1)
    for form in (0, 1):
        for detailed in (False, True):
            keep = (forms == form) & (detail == detailed) & np.isfinite(height)
            if keep.any():
                yield (
                    "redwood fern",
                    *scatter_mesh(
                        sword_fern(form, detailed),
                        anchors[keep],
                        2 * np.pi * marks[keep, 0],
                        scale[keep],
                    ),
                )
