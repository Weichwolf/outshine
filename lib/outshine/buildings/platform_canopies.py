"""Recover open platform roofs when vector tiles omit the building subtype."""

import numpy as np
import shapely

from outshine.terrain.ground_surface import polygons
from outshine.transport.bridges import slab
from outshine.transport.rail_approaches import rail_neighbors

MINIMUM_PLATFORM_OVERLAP = 0.8
MINIMUM_ROOF_ASPECT = 8.0
SUPPORT_SPACING = 12.0
SUPPORT_RADIUS = 0.16
ROOF_SKIN = 0.24


def platform_canopies(features, footprints):
    platforms = shapely.STRtree(
        [
            p
            for name, geom, tags in features
            if name == "transportation" and tags.get("subclass") == "platform"
            for p in polygons(shapely.make_valid(geom))
        ]
    )
    if not len(platforms.geometries):
        return {}
    _, tracks = rail_neighbors(features)
    canopies = {}
    for footprint, tags in footprints:
        if (
            tags.get("render_min_height", 0) != 0
            or not 3.5 <= tags.get("render_height", 0) <= 7.0
        ):
            continue
        candidates = platforms.geometries[platforms.query(footprint, "intersects")]
        if not len(candidates):
            continue
        cover = shapely.area(shapely.intersection(footprint, candidates))
        match = candidates[cover.argmax()]
        if cover.max() < MINIMUM_PLATFORM_OVERLAP * footprint.area:
            continue
        box = np.asarray(footprint.minimum_rotated_rectangle.exterior.coords)[:4]
        edges = np.diff(box[:3], axis=0)
        lengths = np.linalg.norm(edges, axis=1)
        span, width = max(lengths), min(lengths)
        if span < MINIMUM_ROOF_ASPECT * width:
            continue
        axis = edges[lengths.argmax()] / span
        across = axis[::-1] * [-1, 1]
        rails = tracks.geometries[tracks.query(footprint, "dwithin", width)]
        if not len(rails):
            continue
        parallel = []
        centre = box.mean(axis=0)
        for line in rails:
            points = shapely.get_coordinates(line)
            offset = (points - centre) @ across
            if abs((points[-1] - points[0]) @ axis) >= 0.75 * line.length:
                parallel.append(float(np.median(offset)))
        if parallel and min(parallel) < -width / 2 < width / 2 < max(parallel):
            canopies[footprint] = (match, axis, float(tags["render_height"]))
    return canopies


def draw_platform_canopy(meshes, outline, recipe, extent, height, rail_mesh):
    platform, axis, roof_height = recipe
    for roof in polygons(outline.intersection(extent)):
        xy = shapely.get_coordinates(roof)
        along = xy @ axis
        lo, hi = along.min(), along.max()
        if hi - lo < 4:
            continue
        centre = np.asarray(roof.representative_point().coords)[0]
        stations = np.arange(lo + SUPPORT_SPACING / 2, hi, SUPPORT_SPACING)
        points = centre + (stations - centre @ axis)[:, None] * axis
        supports = shapely.buffer(shapely.points(points), SUPPORT_RADIUS, quad_segs=2)
        platform_floor = roof.intersection(platform)
        supports = supports[shapely.covers(platform_floor, supports)]
        if not len(supports):
            foot = platform_floor.representative_point().buffer(SUPPORT_RADIUS)
            if not platform_floor.covers(foot):
                continue
            supports = [foot]
        top = max(height(xy).max(), float(height(centre[None, :])[0])) + roof_height
        if rail_mesh is not None:
            track_points = np.asarray(rail_mesh[0])
            points = shapely.points(track_points[:, :2])
            inside = shapely.covers(roof, points)
            if inside.any():
                top = max(top, track_points[inside, 2].max() + 4.5 + ROOF_SKIN)
        slab(
            meshes,
            "bridge deck",
            roof,
            lambda points, z=top: np.full(len(points), z),
            ROOF_SKIN,
        )
        for post in supports:
            bottom = float(height(shapely.get_coordinates(post)).min())
            slab(
                meshes,
                "bridge deck",
                post,
                lambda points, z=top - ROOF_SKIN: np.full(len(points), z),
                top - ROOF_SKIN - bottom,
            )
