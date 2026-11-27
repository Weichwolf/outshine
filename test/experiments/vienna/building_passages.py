"""Recover covered pedestrian connections from mapped tunnel and approach segments."""

from dataclasses import dataclass

import numpy as np
import shapely
from ground_surface import at_grade_width, route_area

JOIN_TOLERANCE_M = 0.6
PORTAL_OVERLAP_M = 0.9
PORTAL_WIDTH_M = 2.4
VAULT_RISE_M = 0.85
VAULT_HEIGHT_M = 3.0


def passage_axis(passage):
    xy = shapely.get_coordinates(passage.route)
    origin, end = xy[[0, -1]]
    baseline = end - origin
    length = np.linalg.norm(baseline)
    if length < PORTAL_WIDTH_M:
        return
    along = baseline / length
    normal = np.array([-along[1], along[0]])
    if np.max(np.abs((xy - origin) @ normal)) > 1e-6:
        return
    return origin, end, along, normal


def vault_profile(crown):
    radius = PORTAL_WIDTH_M / 2
    angle = np.linspace(-np.pi / 2, np.pi / 2, 25)
    radial = np.column_stack((np.sin(angle), np.cos(angle)))
    inner = radial * [radius, VAULT_RISE_M] + [0, crown - VAULT_RISE_M]
    return inner, radial


def vault_panels(passage, cut, crown):
    frame = passage_axis(passage)
    if frame is None:
        return
    origin, end, along, normal = frame
    lateral, elevation = vault_profile(crown)[0].T
    start = origin - PORTAL_OVERLAP_M * along
    stop = end + PORTAL_OVERLAP_M * along
    for lo, hi, lo_z, hi_z in zip(
        lateral[:-1], lateral[1:], elevation[:-1], elevation[1:], strict=True
    ):
        panel = shapely.Polygon(
            [
                start + lo * normal,
                stop + lo * normal,
                stop + hi * normal,
                start + hi * normal,
            ]
        )
        slope = (hi_z - lo_z) / (hi - lo)
        yield cut.intersection(panel), normal, lo_z, (origin @ normal + lo), slope


@dataclass(frozen=True)
class BuildingPassage:
    route: shapely.LineString
    corridor: shapely.Polygon
    properties: dict
    crown_m: float


def building_passages(transport, footprints, extent, height):
    if not footprints:
        return {}
    buildings = shapely.STRtree(footprints)
    walks = shapely.STRtree(
        [
            line
            for geometry, tags in transport
            if tags.get("class") == "path" and at_grade_width(tags)
            for line in shapely.get_parts(geometry)
            if line.geom_type == "LineString"
        ]
    )
    passages = {}
    for geometry, tags in transport:
        if tags.get("class") != "path" or tags.get("brunnel") != "tunnel":
            continue
        for line in shapely.get_parts(geometry.intersection(extent)):
            if line.geom_type != "LineString" or line.length < PORTAL_WIDTH_M:
                continue
            xy = shapely.get_coordinates(shapely.remove_repeated_points(line))
            ends = xy[[0, -1]]
            contacts, _ = walks.query(shapely.points(ends), "dwithin", JOIN_TOLERANCE_M)
            if len(np.unique(contacts)) != 2:
                continue
            directions = ends - xy[[1, -2]]
            exterior = ends + PORTAL_OVERLAP_M * (
                directions / np.linalg.norm(directions, axis=1)[:, None]
            )
            if buildings.query(shapely.points(exterior), "intersects").size:
                continue
            for footprint in buildings.geometries[buildings.query(line, "intersects")]:
                if not footprint.buffer(JOIN_TOLERANCE_M).covers(line):
                    continue
                corridor = route_area(
                    shapely.LineString(np.vstack([exterior[0], xy, exterior[1]])),
                    PORTAL_WIDTH_M,
                )
                floor_tags = tags | {"brunnel": None, "layer": 0}
                samples = shapely.get_coordinates(
                    shapely.segmentize(
                        shapely.GeometryCollection([line, corridor.boundary]), 1
                    )
                )
                passages.setdefault(footprint, []).append(
                    BuildingPassage(
                        line,
                        corridor,
                        floor_tags,
                        float(height(samples).max()) + VAULT_HEIGHT_M,
                    )
                )
    return passages
