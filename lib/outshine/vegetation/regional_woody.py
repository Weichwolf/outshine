"""Place sparse savanna crowns from the atlas on solved site-local terrain."""

import numpy as np
import shapely

from outshine.terrain.ground_surface import ROUTE_MATERIALS, surface_elevations
from outshine.vegetation.open_cover import (
    ACACIA_COMMIPHORA_BUSHLANDS,
    mesh_footprint,
    planting_surface,
)
from outshine.vegetation.plant_sites import patch_density, tile_sites
from outshine.vegetation.umbrella_thorn import PROFILE_ID, TAXON_ID, umbrella_thorn
from outshine.vegetation.woodland_sites import spaced_sites

SAVANNA_REACH_M = 225.0
MIN_SPACING_M = 8.5
DETAILED_REACH_M = 49.0
VARIANTS = 4


def savanna_woody(cover, surfaces, eye_xy, seed):
    source = cover.source
    reach = SAVANNA_REACH_M + MIN_SPACING_M
    candidates = list(
        tile_sites(source, eye_xy, reach, seed ^ 0xE421, density=0.0018, cell_size=16)
    )
    if not candidates:
        return
    xy, marks = (np.concatenate(rows) for rows in zip(*candidates, strict=True))
    open_land, ecoregion = cover.open_habitat(xy, codes=(10, 20, 30))
    suitable = open_land & (ecoregion == ACACIA_COMMIPHORA_BUSHLANDS)
    xy, marks = xy[suitable], marks[suitable]
    if not len(xy):
        return
    lon, lat = (xy / source.metres + [source.lon, source.lat]).T
    native = np.array(
        [
            TAXON_ID in cover.atlas.sample(y, x).native_candidates
            for y, x in zip(lat, lon, strict=True)
        ]
    )
    patch = patch_density(source, xy, seed ^ 0x243A, scale=21.0)
    keep = native & (marks[:, 2] < 0.17 + 0.72 * patch**1.65)
    xy, marks = xy[keep], marks[keep]
    if not len(xy):
        return
    ground = planting_surface(surfaces, eye_xy, reach=reach)
    if ground is None:
        return
    footprint = mesh_footprint(ground)
    routes = planting_surface(surfaces, eye_xy, ROUTE_MATERIALS, reach=reach + 6)
    if routes is not None:
        footprint = footprint.difference(mesh_footprint(routes).buffer(4.0))
    points = shapely.points(xy)
    heights = surface_elevations(ground, xy)
    supported = np.isfinite(heights) & shapely.contains(footprint, points)
    supported &= shapely.distance(footprint.boundary, points) > 0.75
    xy, marks, heights = (v[supported] for v in (xy, marks, heights))
    if not len(xy):
        return
    distance = np.linalg.norm(xy - eye_xy, axis=1)
    selected = spaced_sites(xy, marks[:, 0], MIN_SPACING_M)
    selected &= distance < SAVANNA_REACH_M
    xy, marks, heights, distance = (v[selected] for v in (xy, marks, heights, distance))
    mature = marks[:, 1] >= 0.24
    scale = np.where(mature, 0.81 + 0.57 * marks[:, 0], 0.49 + 0.30 * marks[:, 0])
    yaw = np.mod(marks[:, 1] * 11, 1) * np.pi * 2
    variants = (np.mod(marks[:, 0] * 17, 1) * VARIANTS).astype(int)
    detailed = distance < DETAILED_REACH_M * scale
    for age, is_mature in enumerate((False, True)):
        for detail, is_detailed in enumerate((False, True)):
            for variant in range(VARIANTS):
                group = (
                    (mature == is_mature)
                    & (detailed == is_detailed)
                    & (variants == variant)
                )
                if group.any():
                    yield (
                        PROFILE_ID,
                        (age * 2 + detail) * VARIANTS + variant,
                        umbrella_thorn(variant, is_mature, is_detailed),
                        np.column_stack(
                            (xy[group], heights[group], yaw[group], scale[group])
                        ),
                    )
