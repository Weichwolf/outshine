"""Coarse rooted woodland crowns for the regional pedestrian views."""

from functools import cache

import numpy as np
import shapely
from ground_surface import surface_elevations, terrain_triangles
from montane_trees import conifer_sites
from outshine_ref.places.sources import tile_geography
from outshine_ref.vegetation.axis_surface import axis_mesh
from scipy.spatial import ConvexHull

SITE_SPACING_M = 8.0
CROWN_HEIGHT_M = 14.0
MAX_SCALE = 1.30
MIN_CROWN_PIXELS = 8.0
FULL_CROWN_PIXELS = 12.0


def crown_reach(focal):
    return focal * CROWN_HEIGHT_M * MAX_SCALE / MIN_CROWN_PIXELS


def tile_crown_sites(source, tile, seed):
    latitude, longitude = tile_geography([[0, 1], [1, 0]], 1, tile)
    sw, ne = np.column_stack((longitude, latitude))
    metre = np.array((111319.49 * np.cos(np.deg2rad(latitude.mean())), 111132.0))
    span = (ne - sw) * metre
    rows, columns = np.ceil(span / SITE_SPACING_M).astype(int)
    grid = np.indices((rows, columns)).reshape(2, -1).T
    rng = np.random.default_rng(np.random.SeedSequence((seed, *tile)))
    random = rng.random((len(grid), 6))
    positions = (grid + 0.15 + 0.70 * random[:, :2]) * SITE_SPACING_M
    keep = np.all(positions < span, axis=1)
    enu = (sw - [source.lon, source.lat] + positions[keep] / metre) * source.metres
    return enu, random[keep, 2:]


def crown_lobe(offset, girth, profile):
    angle = np.arange(6) * (np.pi / 3)
    rings = []
    for level, (height, radius) in enumerate(profile):
        if not radius:
            rings.append(np.array([[0.12 * level, 0, height]]))
            continue
        phase = angle + 0.31 * level
        radial = radius * np.array([0.94, 1.12, 0.89, 1.04, 0.93, 1.09])
        rings.append(
            np.c_[np.cos(phase) * radial, np.sin(phase) * radial, np.full(6, height)]
        )
    vertices = np.vstack(rings) * girth + offset
    hull = ConvexHull(vertices)
    faces = hull.simplices.copy()
    a, b, c = vertices[faces].transpose(1, 0, 2)
    inverted = np.sum(np.cross(b - a, c - a) * hull.equations[:, :3], axis=1) < 0
    faces[inverted] = faces[inverted, ::-1]
    return vertices, faces


@cache
def crown_template(conifer):
    if conifer:
        lobes = (
            crown_lobe((0, 0, 0), (1, 1, 1), ((2.8, 0), (4.4, 3.2), (9.8, 0))),
            crown_lobe((0.12, 0, 0), (1, 1, 1), ((5.1, 0), (7.4, 2.5), (14, 0))),
        )
    else:
        profile = ((3.6, 0), (6.1, 3.55), (9.5, 3.3), (12.3, 0))
        lobes = (
            crown_lobe((-1.35, 0.4, 0), (1.10, 0.90, 1), profile),
            crown_lobe((1.5, -0.55, 1.7), (0.99, 1.00, 1), profile),
        )
    vertices = np.vstack([vertex for vertex, _ in lobes])
    faces = np.vstack((lobes[0][1], lobes[1][1] + len(lobes[0][0])))
    stem = axis_mesh(7.0, 0.04, 0.28, 0.09, 2, 3)
    upright = stem["vertices"][:, (0, 2, 1)] * (1, -1, 1)
    return (vertices, faces), (upright, stem["faces"])


def rooted_crowns(source, receipt, data, forest, focal, seed):
    xy, marks = tile_crown_sites(source, receipt["tile"], seed)
    scale = 0.68 + (MAX_SCALE - 0.68) * marks[:, 2]
    distance = np.linalg.norm(xy, axis=1)
    projected = focal * CROWN_HEIGHT_M * scale / np.maximum(distance, 1.0)
    visibility = np.clip(
        (projected - MIN_CROWN_PIXELS) / (FULL_CROWN_PIXELS - MIN_CROWN_PIXELS), 0, 1
    )
    keep = marks[:, 3] < visibility
    if not keep.any():
        return
    xy, marks, scale = xy[keep], marks[keep], scale[keep]
    height = np.full(len(xy), -np.inf)
    for vertices, faces in forest:
        triangles = terrain_triangles(faces) if faces.shape[1] == 4 else faces
        height = np.maximum(height, surface_elevations((vertices, triangles), xy))
    keep = np.isfinite(height)
    if not keep.any():
        return
    xy, marks, scale, height = xy[keep], marks[keep], scale[keep], height[keep]
    features = list(source.metric_features(receipt, ("building", "landcover"), data))
    buildings = [shapely.make_valid(g) for name, g, _ in features if name == "building"]
    if buildings:
        overlap = shapely.STRtree(buildings).query(
            shapely.points(xy), predicate="intersects"
        )[0]
        keep = np.ones(len(xy), dtype=bool)
        keep[overlap] = False
        xy, marks, scale, height = xy[keep], marks[keep], scale[keep], height[keep]
    conifers = conifer_sites(source, xy, height, marks[:, 0], features=features)
    anchors = np.c_[xy, height - 0.10]
    placements = np.c_[anchors, 2 * np.pi * marks[:, 1], scale].astype(np.float32)
    for needle, selected in ((False, ~conifers), (True, conifers)):
        if selected.any():
            yield needle, placements[selected]


def crown_batches(sites):
    for conifer, species, canopy in (
        (False, "Regional broadleaf", "regional wood"),
        (True, "Regional spruce", "spruce foliage"),
    ):
        groups = [placements for needle, placements in sites if needle == conifer]
        if groups:
            crown, stem = crown_template(conifer)
            yield species, {canopy: crown, "shrub wood": stem}, np.concatenate(groups)
