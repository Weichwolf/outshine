"""Excavate terrain that passes through the deck at a mapped bridgehead."""

from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.transport.water_span import WaterSpan


@dataclass(frozen=True)
class BankCut:
    footprint: shapely.Geometry
    deck: WaterSpan
    depth: float

    def heights(self, ground):
        return np.minimum(ground[:, 2], self.deck.height(ground[:, :2]) - self.depth)


def above_deck(ground, difference):
    above = difference > 0
    full = above.all(axis=1)
    polygons = list(shapely.polygons(ground[full, :, :2]))
    for vertices, distances in zip(ground[~full], difference[~full], strict=True):
        points = []
        for i, start in enumerate(vertices):
            j = (i + 1) % 3
            if distances[i] > 0:
                points.append(start[:2])
            if (distances[i] > 0) != (distances[j] > 0):
                t = distances[i] / (distances[i] - distances[j])
                points.append(start[:2] + t * (vertices[j, :2] - start[:2]))
        if len(points) >= 3:
            polygons.append(shapely.Polygon(points))
    return shapely.union_all(polygons, grid_size=1e-6)


def bank_cuts(vertices, quads, crossings, extent):
    if not crossings:
        return
    cells = vertices[quads]
    xy = cells[:, :, :2]
    minimum, maximum = xy.min(axis=1), xy.max(axis=1)
    for crossing in crossings:
        region = crossing.footprint.intersection(extent)
        if region.is_empty:
            continue
        lo, hi = np.asarray(region.bounds).reshape(2, 2)
        hits = (maximum >= lo).all(axis=1) & (minimum <= hi).all(axis=1)
        triangles = cells[hits][:, [0, 1, 2, 0, 2, 3]].reshape(-1, 3, 3)
        xy = triangles[:, :, :2]
        surface = crossing.span.height(xy.reshape(-1, 2)).reshape(-1, 3)
        difference = triangles[:, :, 2] - surface + 0.01
        high = (difference > 0).any(axis=1)
        floor = above_deck(triangles[high], difference[high]).intersection(region)
        if floor.area > 0:
            yield BankCut(floor, crossing.span, crossing.thickness + 0.02)
