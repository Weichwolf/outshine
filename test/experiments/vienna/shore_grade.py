"""Grade terrain into the mapped WGS84 water contact."""

from dataclasses import dataclass
from functools import cached_property

import numpy as np
import shapely

SHORE_REACH_M = 14.0
CONTACT_TOLERANCE_M = 2e-6


@dataclass(frozen=True)
class ShoreProtection:
    footprint: shapely.Geometry

    @cached_property
    def index(self):
        shapely.prepare(self.footprint)
        index, _ = segment_index((self.footprint.boundary,))
        return index

    def weight(self, points, xy, distance):
        weights = np.ones(len(points))
        if not len(self.index):
            return weights
        inside = shapely.contains_xy(self.footprint, *xy.T)
        weights[inside] = 0
        outside = np.flatnonzero(~inside)
        matched, clearance = self.index.query_nearest(
            points[outside],
            max_distance=SHORE_REACH_M,
            all_matches=False,
            return_distance=True,
        )
        clearance[clearance <= CONTACT_TOLERANCE_M] = 0
        active = outside[matched[0]]
        span = clearance + distance[active]
        protection = np.divide(clearance, span, out=np.zeros_like(span), where=span > 0)
        fade = smoothstep(clearance / SHORE_REACH_M)
        weights[active] = fade + (1 - fade) * smoothstep(protection)
        return weights


@dataclass(frozen=True)
class ShoreGrade:
    bodies: tuple
    contacts: tuple
    protection: ShoreProtection
    neighbor: tuple[ShoreProtection, shapely.Geometry] | None = None

    @cached_property
    def coast_segments(self):
        return segment_index(self.contacts)

    def breaks(self, wet, extent):
        if not self.contacts:
            return []
        shore = shapely.union_all(self.contacts)
        contours = shapely.boundary(
            shapely.buffer(shore, (0.8, 2.5, 6.0, SHORE_REACH_M), quad_segs=3)
        )
        dry = extent.difference(wet, grid_size=1e-6)
        return shapely.intersection(contours, dry, grid_size=1e-6).tolist()

    def heights(self, source, ground):
        if not len(ground) or not self.contacts:
            return ground[:, 2]
        points = shapely.points(ground[:, :2])
        index, owners = self.coast_segments
        indices, distances = index.query_nearest(
            points, max_distance=SHORE_REACH_M, return_distance=True
        )
        body_numbers = owners[indices[1]]
        order = np.lexsort((body_numbers, distances, indices[0]))
        _, first = np.unique(indices[0, order], return_index=True)
        chosen = order[first]
        vertices, bodies = indices[0, chosen], body_numbers[chosen]
        distances = distances[chosen]
        distance = np.maximum(distances - CONTACT_TOLERANCE_M, 0)
        influence = 1 - smoothstep(distance / SHORE_REACH_M)
        bank_points = points[vertices]
        xy = ground[vertices, :2]
        weights = self.protection.weight(bank_points, xy, distance)
        if self.neighbor is not None:
            protection, seam = self.neighbor
            seam_distance = shapely.distance(bank_points, seam)
            uses = seam_distance < SHORE_REACH_M
            blend = 1 - smoothstep(seam_distance[uses] / SHORE_REACH_M)
            local = protection.weight(bank_points[uses], xy[uses], distance[uses])
            weights[uses] += blend * (local - weights[uses])
        influence *= weights
        heights = ground[:, 2].copy()
        for i in np.unique(bodies):
            uses = bodies == i
            rows = vertices[uses]
            level = self.bodies[i].heights(source, ground[rows, :2])
            heights[rows] += influence[uses] * (level - heights[rows])
        return heights

    def deform(self, source, ground):
        if not self.contacts:
            return ground
        return np.column_stack((ground[:, :2], self.heights(source, ground)))

    def shoreline_edges(self, banks):
        if not self.contacts or not len(banks):
            return banks[:0]
        midpoints = shapely.points(banks[:, :, :2].mean(axis=1))
        index, _ = self.coast_segments
        hits, _ = index.query(
            midpoints, predicate="dwithin", distance=CONTACT_TOLERANCE_M
        )
        return banks[np.unique(hits)]


def smoothstep(t):
    return t * t * (3 - 2 * t)


def segment_index(geometries):
    parts, owners = shapely.get_parts(geometries, return_index=True)
    parts, parent = shapely.get_parts(parts, return_index=True)
    points, lines = shapely.get_coordinates(parts, return_index=True)
    starts = np.flatnonzero(lines[:-1] == lines[1:])
    edges = shapely.linestrings(points[starts[:, None] + [0, 1]])
    return shapely.STRtree(edges), owners[parent[lines[starts]]]


def shore_grade(bodies, extent, protected, *, neighbor=None):
    protection = ShoreProtection(protected)
    neighborhood = extent.buffer(SHORE_REACH_M + 1.0, quad_segs=2)
    nearby = tuple(b for b in bodies if b.footprint.intersects(neighborhood))
    if not nearby:
        return ShoreGrade((), (), protection)
    clipped = shapely.intersection(
        [b.footprint for b in nearby], neighborhood, grid_size=1e-6
    )
    wet = shapely.union_all(clipped, grid_size=1e-6)
    coast = wet.boundary.difference(neighborhood.boundary, grid_size=1e-6)
    contacts = shapely.intersection(shapely.boundary(clipped), coast, grid_size=1e-6)
    active = [
        (body, contact)
        for body, contact in zip(nearby, contacts, strict=True)
        if contact.length > 0
    ]
    return ShoreGrade(
        tuple(body for body, _ in active),
        tuple(contact for _, contact in active),
        protection,
        neighbor,
    )
