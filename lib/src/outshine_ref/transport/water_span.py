"""A straight bridge's bearing line and deck grade from its mapped shore contacts."""

from dataclasses import dataclass

import numpy as np
import shapely
from shapely.geometry import LineString


@dataclass(frozen=True)
class WaterSpan:
    origin: np.ndarray
    direction: np.ndarray
    length: float
    elevation: float
    grade: float

    def height(self, xy):
        return self.elevation + ((xy - self.origin) @ self.direction) * self.grade


def solve_water_span(footprint, routes, water, terrain):
    """Fit a single bank-to-bank deck; leave branching or incomplete crossings unresolved."""
    tracks = [
        part
        for route in routes
        for part in shapely.get_parts(route.intersection(footprint))
        if part.geom_type == "LineString" and part.length > 0
    ]
    if not tracks or not footprint.intersects(water):
        return None
    path = max(tracks, key=lambda line: line.length)
    ends = np.asarray(path.coords)[[0, -1]]
    chord = ends[1] - ends[0]
    length = np.linalg.norm(chord)
    if length < 0.95 * path.length:
        return None
    axis = chord / length
    centre = ends.mean(axis=0)
    diagonal = np.linalg.norm(np.subtract(footprint.bounds[2:], footprint.bounds[:2]))
    bearing = LineString([centre - diagonal * axis, centre + diagonal * axis])
    chords = [
        part
        for part in shapely.get_parts(footprint.intersection(bearing))
        if part.geom_type == "LineString"
        and part.intersects(
            path.interpolate(0.5, normalized=True).buffer(0.05 * path.length)
        )
    ]
    if len(chords) != 1:
        return None
    line = chords[0]
    wet = line.intersection(water)
    if (
        wet.geom_type != "LineString"
        or wet.length == 0
        or not water.covers(line.interpolate(0.5, normalized=True))
    ):
        return None
    ends = np.asarray(line.coords)[[0, -1]]
    length = line.length
    axis = (ends[1] - ends[0]) / length
    # Buffered vector shorelines can run past the deck boundary. Seek dry land
    # along its approaches, within one deck width on each side.
    width = footprint.area / length
    probes = np.linspace(0, width, 17)
    contacts = []
    for end, direction in zip(ends, [-axis, axis], strict=True):
        points = end + probes[:, None] * direction
        dry = ~shapely.intersects_xy(water, *points.T)
        if not dry.any():
            return None
        contacts.append(points[np.argmax(dry)])
    heights = terrain(np.asarray(contacts))
    run = np.dot(contacts[1] - contacts[0], axis)
    grade = float((heights[1] - heights[0]) / run)
    elevation = float(heights[0] + np.dot(ends[0] - contacts[0], axis) * grade)
    return WaterSpan(ends[0], axis, length, elevation, grade)
