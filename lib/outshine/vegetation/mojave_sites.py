"""Place Mojave woody plants where the flora, cover and solved terrain agree."""

from collections.abc import Callable
from dataclasses import dataclass

import numpy as np
import shapely

from outshine.terrain.ground_surface import (
    ROUTE_MATERIALS,
    SOIL_MATERIALS,
    surface_elevations,
)
from outshine.vegetation.blackbrush import (
    BLACKBRUSH_PROFILE,
    BLACKBRUSH_TAXON,
    BLACKBRUSH_UNITS,
    blackbrush,
)
from outshine.vegetation.muller_oak import (
    MULLER_OAK_PROFILE,
    MULLER_OAK_TAXON,
    MULLER_OAK_UNITS,
    muller_oak,
)
from outshine.vegetation.open_cover import (
    MOJAVE_DESERT,
    OPEN_SURFACES,
    mesh_footprint,
    planting_surface,
)
from outshine.vegetation.pinyon import (
    PINYON_PROFILE,
    PINYON_TAXON,
    PINYON_UNITS,
    pinyon,
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
BLACKBRUSH_REACH_M = 122.0
BLACKBRUSH_SPACING_M = 0.86


@dataclass(frozen=True)
class ObservedPlant:
    taxon: str
    profile: str
    units: frozenset[str]
    geometry: Callable
    reach: float
    spacing: float
    candidates_m2: float
    patch_m: float
    fine_reach: float
    salt: int
    scale: tuple[float, float] = (0.65, 1.32)
    burial_m: float = 0.03


BLACKBRUSH = ObservedPlant(
    BLACKBRUSH_TAXON,
    BLACKBRUSH_PROFILE,
    BLACKBRUSH_UNITS,
    blackbrush,
    BLACKBRUSH_REACH_M,
    BLACKBRUSH_SPACING_M,
    0.26,
    4.7,
    22,
    0x396E,
)
WOODLAND = (
    ObservedPlant(
        PINYON_TAXON,
        PINYON_PROFILE,
        PINYON_UNITS,
        pinyon,
        reach=214,
        spacing=4.8,
        candidates_m2=0.0145,
        patch_m=10,
        fine_reach=34,
        salt=0x1BA4,
        scale=(0.69, 1.15),
        burial_m=0.0,
    ),
    ObservedPlant(
        MULLER_OAK_TAXON,
        MULLER_OAK_PROFILE,
        MULLER_OAK_UNITS,
        muller_oak,
        reach=150,
        spacing=1.9,
        candidates_m2=0.035,
        patch_m=6.2,
        fine_reach=24,
        salt=0x852A,
        scale=(0.67, 1.23),
        burial_m=0.065,
    ),
)


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
        supported = rooted_sites(cover, surfaces, eye_xy, halo, xy, marks)
        if supported is not None:
            yield from yucca_batches(taxon, profile, *supported, eye_xy, reach, spacing)


def rooted_sites(cover, surfaces, eye_xy, halo, xy, marks, *, materials=OPEN_SURFACES):
    if not len(xy):
        return None
    ground = planting_surface(surfaces, eye_xy, materials, reach=halo + 4)
    if ground is None:
        return None
    domain = mesh_footprint(ground)
    routes = planting_surface(surfaces, eye_xy, ROUTE_MATERIALS, reach=halo + 4)
    if routes is not None:
        domain = domain.difference(mesh_footprint(routes))
    if cover.exclusion is not None:
        domain = domain.difference(cover.exclusion)
    heights = surface_elevations(ground, xy)
    supported = np.isfinite(heights) & shapely.contains_xy(domain, *xy.T)
    if not supported.any():
        return None
    xy, marks, heights = (values[supported] for values in (xy, marks, heights))
    clearance = shapely.distance(domain.boundary, shapely.points(xy))
    return xy, marks, heights, clearance


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


def observed_sites(cover, plant, eye_xy, halo, seed):
    candidates = list(
        tile_sites(
            cover.source, eye_xy, halo, seed, density=plant.candidates_m2, cell_size=8
        )
    )
    if not candidates:
        return np.empty((0, 2)), np.empty((0, 3))
    xy, marks = (np.concatenate(rows) for rows in zip(*candidates, strict=True))
    land, ecoregions = cover.land_cells(xy, include_wood=True)
    lon, lat = cover.coordinates(xy)
    indices, density = cover.atlas.observed_cells(lat, lon)
    suitable = (
        (ecoregions == MOJAVE_DESERT)
        & np.isin(land, (10, 20, 30, 60))
        & cover.atlas.observation_mask(indices, plant.units)
    )
    xy, marks, density = (values[suitable] for values in (xy, marks, density))
    if not len(xy):
        return xy, marks
    patch = patch_density(cover.source, xy, seed ^ 0x6B11, scale=plant.patch_m)
    # Source classes describe total vegetation cover, with 5 denoting 2–10%.
    # Convert the category to a prior for this shrub's share.
    weight = np.select(
        (density == 5, density == 4, np.isin(density, (1, 2, 3))),
        (0.34, 0.66, 0.91),
    )
    keep = marks[:, 2] < weight * (0.14 + 0.77 * patch**1.28)
    xy, marks = xy[keep], marks[keep]
    lon, lat = cover.coordinates(xy)
    native = np.fromiter(
        (
            plant.taxon in cover.atlas.sample(y, x).native_candidates
            for y, x in zip(lat, lon, strict=True)
        ),
        dtype=bool,
        count=len(xy),
    )
    return xy[native], marks[native]


def observed_plants(cover, plant, surfaces, eye_xy, seed):
    halo = plant.reach + plant.spacing
    lon, lat = cover.coordinates(np.array([eye_xy - halo, eye_xy + halo]))
    bounds = (lon[0], lat[0], lon[1], lat[1])
    if not any(cover.atlas.evidence_windows(bounds, "observed_vegetation")):
        return
    xy, marks = observed_sites(cover, plant, eye_xy, halo, seed ^ plant.salt)
    supported = rooted_sites(
        cover, surfaces, eye_xy, halo, xy, marks, materials=SOIL_MATERIALS
    )
    if supported is None:
        return
    xy, marks, heights, clearance = supported
    distance = np.linalg.norm(xy - eye_xy, axis=1)
    variant = np.floor(VARIANTS * np.mod(17 * marks[:, 0], 1)).astype(int)
    lo, hi = plant.scale
    scale = lo + (hi - lo) * np.mod(11 * marks[:, 0], 1)
    angles = 2 * np.pi * marks[:, 1]
    fine = distance < plant.fine_reach * scale
    geometries = {
        v: tuple(plant.geometry(int(v), high) for high in (False, True))
        for v in np.unique(variant)
    }
    radii = {v: max(radius for _, radius in lods) for v, lods in geometries.items()}
    bounds = np.array([radii[v] for v in variant]) * scale
    fits = clearance > bounds + 0.09
    candidates = np.flatnonzero(fits)
    chosen = candidates[
        spaced_sites(xy[candidates], marks[candidates, 0], plant.spacing)
    ]
    visible = np.zeros(len(xy), dtype=bool)
    visible[chosen] = distance[chosen] < plant.reach
    for v, lods in geometries.items():
        for lod, (meshes, _) in enumerate(lods):
            group = visible & (variant == v) & (fine == bool(lod))
            if group.any():
                yield (
                    plant.profile,
                    int(2 * v + lod),
                    meshes,
                    np.column_stack(
                        (
                            xy[group],
                            heights[group] - plant.burial_m,
                            angles[group],
                            scale[group],
                        )
                    ),
                )


def mojave_blackbrush(cover, surfaces, eye_xy, seed):
    yield from observed_plants(cover, BLACKBRUSH, surfaces, eye_xy, seed)


def mojave_woodland(cover, surfaces, eye_xy, seed):
    for plant in WOODLAND:
        yield from observed_plants(cover, plant, surfaces, eye_xy, seed)
