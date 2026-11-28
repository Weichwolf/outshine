"""Sample a mapped footpath against the solved render mesh."""

from dataclasses import dataclass

import numpy as np
import shapely
from ground_surface import at_grade_width
from reference_scene import support_height
from shapely.geometry import LineString, Point
from shapely.ops import substring


@dataclass(frozen=True)
class Walk:
    route: LineString
    eyes: np.ndarray
    spacing_m: float
    foot_heights: np.ndarray


def mapped_footpath(features, near):
    lines = [
        line
        for layer, shape, tags in features
        if layer == "transportation"
        and tags.get("class") == "path"
        and at_grade_width(tags) is not None
        for line in shapely.get_parts(shape)
        if line.geom_type == "LineString" and line.length > 0
    ]
    if not lines:
        raise ValueError("no mapped at-grade footpath for the walk")
    point = Point(near)
    return min(lines, key=lambda line: line.distance(point))


def projected_support(mesh, bounds):
    vertices, faces = mesh
    polygons = np.asarray(vertices)[np.asarray(faces, dtype=int), :2]
    if not len(polygons):
        return shapely.Polygon()
    lo, hi = np.asarray(bounds).reshape(2, 2)
    overlap = np.all(
        (polygons.min(axis=1) <= hi) & (polygons.max(axis=1) >= lo), axis=1
    )
    footprints = shapely.polygons(polygons[overlap])
    return shapely.union_all(footprints[shapely.area(footprints) > 1e-12])


def plan_walk(world, config, profile):
    path = mapped_footpath(world.source.features, world.eye[:2])
    origin = path.project(Point(world.eye[:2]))
    start, end = origin + np.asarray(profile["station_offsets_m"], dtype=float)
    step = float(profile["step_m"])
    if not (
        0 <= min(start, end) < max(start, end) <= path.length and 0 < step < np.inf
    ):
        raise ValueError(
            "walk stations and step must define a section of one mapped path"
        )
    route = substring(path, start, end)
    mesh = world.local[world.support]
    tolerance = 1e-7
    bounds = np.asarray(route.bounds) + tolerance * np.array([-1, -1, 1, 1])
    if not projected_support(mesh, bounds).buffer(tolerance).covers(route):
        raise ValueError("walk crosses a gap in the solved support mesh")

    def stations(resolution):
        count = max(1, int(np.ceil(route.length / resolution - 1e-9)))
        distance = np.linspace(0, route.length, count + 1)
        return shapely.get_coordinates(shapely.line_interpolate_point(route, distance))

    def elevation(points):
        return np.array([support_height(mesh, xy).max() for xy in points])

    xy = stations(step)
    height = elevation(xy) + config["eye_height_m"]
    return Walk(
        route,
        np.column_stack((xy, height)),
        route.length / (len(xy) - 1),
        elevation(stations(0.1)),
    )
