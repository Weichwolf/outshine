"""Place Mojave yuccas where the flora, cover and solved terrain agree."""

import numpy as np
import shapely

from outshine.terrain.ground_surface import ROUTE_MATERIALS, surface_elevations
from outshine.vegetation.open_cover import (
    MOJAVE_DESERT,
    mesh_footprint,
    planting_surface,
)
from outshine.vegetation.plant_sites import patch_density, tile_sites
from outshine.vegetation.woodland_sites import spaced_sites
from outshine.vegetation.yuccas import (
    JOSHUA_PROFILE,
    JOSHUA_TAXON,
    MOJAVE_YUCCA_PROFILE,
    MOJAVE_YUCCA_TAXON,
    yucca_geometry,
)

YUCCA_POPULATIONS = (
    (JOSHUA_TAXON, JOSHUA_PROFILE, 215.0, 3.7, 0.009, 0x458A),
    (MOJAVE_YUCCA_TAXON, MOJAVE_YUCCA_PROFILE, 135.0, 1.6, 0.033, 0x2859),
)
VARIANTS = 4


def habitat_sites(cover, eye_xy, seed, reach, density, taxon):
    candidates = list(
        tile_sites(cover.source, eye_xy, reach, seed, density=density, cell_size=12)
    )
    if not candidates:
        return np.empty((0, 2)), np.empty((0, 3))
    xy, marks = (np.concatenate(rows) for rows in zip(*candidates, strict=True))
    land, ecoregion = cover.land_cells(xy)
    suitable = (ecoregion == MOJAVE_DESERT) & np.isin(land, (20, 30, 60))
    xy, marks, land = (values[suitable] for values in (xy, marks, land))
    if not len(xy):
        return xy, marks
    joshua = taxon == JOSHUA_TAXON
    patch = patch_density(cover.source, xy, seed ^ 0x7CB6, scale=12 if joshua else 6)
    probability = np.where(land == 60, 0.53, 1.0) * (0.14 + 0.89 * patch**1.26)
    keep = marks[:, 2] < probability
    xy, marks = xy[keep], marks[keep]
    if not len(xy):
        return xy, marks
    lon, lat = cover.coordinates(xy)
    native = np.fromiter(
        (
            taxon in cover.atlas.sample(y, x).native_candidates
            for y, x in zip(lat, lon, strict=True)
        ),
        dtype=bool,
        count=len(xy),
    )
    offsets = np.array([[3, 0], [-3, 0], [0, 3], [0, -3]])
    east, west, north, south = (
        cover.source.height((xy[:, None, :] + offsets).reshape(-1, 2)).reshape(-1, 4).T
    )
    slope = np.hypot(east - west, north - south) / 6.0
    allowed = native & (slope < (0.62 if joshua else 0.85))
    return xy[allowed], marks[allowed]


def mojave_yuccas(cover, surfaces, eye_xy, seed):
    for taxon, profile, reach, spacing, density, salt in YUCCA_POPULATIONS:
        halo = reach + spacing
        xy, marks = habitat_sites(cover, eye_xy, seed ^ salt, halo, density, taxon)
        if not len(xy):
            continue
        ground = planting_surface(surfaces, eye_xy, reach=halo + 4)
        if ground is None:
            continue
        domain = mesh_footprint(ground)
        routes = planting_surface(surfaces, eye_xy, ROUTE_MATERIALS, reach=halo + 4)
        if routes is not None:
            domain = domain.difference(mesh_footprint(routes))
        if cover.exclusion is not None:
            domain = domain.difference(cover.exclusion)
        heights = surface_elevations(ground, xy)
        supported = np.isfinite(heights) & shapely.contains_xy(domain, *xy.T)
        xy, marks, heights = (values[supported] for values in (xy, marks, heights))
        if not len(xy):
            continue
        clearance = shapely.distance(domain.boundary, shapely.points(xy))
        yield from yucca_batches(
            taxon, profile, xy, marks, heights, clearance, eye_xy, reach, spacing
        )


def yucca_batches(
    taxon, profile, xy, marks, heights, clearance, eye_xy, reach, spacing
):
    joshua = taxon == JOSHUA_TAXON
    distance = np.linalg.norm(xy - eye_xy, axis=1)
    variants = (VARIANTS * np.mod(17 * marks[:, 0], 1)).astype(int)
    mature = (marks[:, 1] >= 0.30) if joshua else np.zeros(len(xy), dtype=bool)
    angles = 2 * np.pi * np.mod(13 * marks[:, 1], 1)
    scales = 0.67 + 0.65 * np.mod(7 * marks[:, 0], 1)
    detailed = distance < (48 if joshua else 30) * scales
    rows = np.column_stack((xy, heights, angles, scales))
    cohorts = mature.astype(int) * VARIANTS + variants
    geometries = {
        cohort: tuple(
            yucca_geometry(taxon, cohort % VARIANTS, cohort >= VARIANTS, high)
            for high in (False, True)
        )
        for cohort in np.unique(cohorts)
    }
    radii = {
        cohort: max(radius for _, radius in lods) for cohort, lods in geometries.items()
    }
    fits = clearance > 0.14 + scales * np.array([radii[cohort] for cohort in cohorts])
    keep = np.flatnonzero(fits)
    chosen = keep[spaced_sites(xy[keep], marks[keep, 0], spacing)]
    visible = np.zeros(len(xy), dtype=bool)
    visible[chosen] = distance[chosen] < reach
    for cohort, lods in geometries.items():
        for detail, (meshes, _) in enumerate(lods):
            draw = visible & (cohorts == cohort) & (detailed == bool(detail))
            if draw.any():
                yield profile, int(2 * cohort + detail), meshes, rows[draw]
