"""Patch-weighted stand sites with locally inhibited stems."""

import numpy as np
import shapely
from plant_sites import patch_density, tile_sites

RECRUIT_CANDIDATES_PER_M2 = 0.08
CANOPY_CANDIDATES_PER_M2 = 0.012
MIN_STEM_SPACING_M = 2.9
EDGE_REGENERATION_M = 8.0


def spaced_sites(xy, rank, spacing):
    points = shapely.points(xy)
    owner, neighbour = shapely.STRtree(points).query(
        points, predicate="dwithin", distance=spacing
    )
    left = xy[neighbour, 0] < xy[owner, 0]
    below = (xy[neighbour, 0] == xy[owner, 0]) & (xy[neighbour, 1] < xy[owner, 1])
    duplicate = np.all(xy[neighbour] == xy[owner], axis=1) & (neighbour < owner)
    better = (rank[neighbour] < rank[owner]) | (
        (rank[neighbour] == rank[owner]) & (left | below | duplicate)
    )
    keep = np.ones(len(xy), dtype=bool)
    keep[owner[better]] = False
    return keep


def cohort_sites(source, domain, centre, half_extent, seed, *, canopy):
    candidates = list(
        tile_sites(
            source,
            centre,
            np.sqrt(2) * half_extent + MIN_STEM_SPACING_M,
            seed if canopy else seed ^ 0x54A7,
            density=CANOPY_CANDIDATES_PER_M2 if canopy else RECRUIT_CANDIDATES_PER_M2,
            cell_size=16,
        )
    )
    if not candidates:
        return np.empty((0, 2)), np.empty((0, 3))
    xy, marks = (np.concatenate(rows) for rows in zip(*candidates, strict=True))
    halo = np.all(np.abs(xy - centre) < half_extent + MIN_STEM_SPACING_M, axis=1)
    xy, marks = xy[halo], marks[halo]
    habitat = shapely.contains_xy(domain, *xy.T)
    xy, marks = xy[habitat], marks[habitat]
    if canopy:
        marks[:, 2] = 0.95 + marks[:, 2] * 0.95
        return xy, marks
    patch = patch_density(source, xy, seed ^ 0x69DF, scale=9.0)
    selection = patch_density(source, xy, seed ^ 0x2F71, scale=0.125)
    edge = shapely.distance(shapely.points(xy), domain.boundary)
    interior = np.minimum(1, edge / EDGE_REGENERATION_M)
    keep = selection < (0.48 + 0.46 * patch) * (0.42 + 0.58 * interior)
    marks[:, 2] = 0.68 + marks[:, 2] * 0.74
    return xy[keep], marks[keep]


def woodland_sites(source, domain, centre, half_extent, seed):
    canopy, canopy_marks = cohort_sites(
        source, domain, centre, half_extent, seed, canopy=True
    )
    young, young_marks = cohort_sites(
        source, domain, centre, half_extent, seed, canopy=False
    )
    xy = np.concatenate((canopy, young))
    marks = np.concatenate((canopy_marks, young_marks))
    rank = patch_density(source, xy, seed ^ 0xA91B, scale=0.125)
    rank[len(canopy) :] += 1
    keep = spaced_sites(xy, rank, MIN_STEM_SPACING_M)
    keep &= np.all(np.abs(xy - centre) < half_extent, axis=1)
    return xy[keep], marks[keep]
