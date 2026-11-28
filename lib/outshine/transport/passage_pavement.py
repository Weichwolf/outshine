"""Lay out grade-constrained pavement triangles with conforming terrain ends."""

from dataclasses import dataclass

import numpy as np
import shapely

from outshine.geometry.sampling import SurfaceSampler
from outshine.terrain.ground_surface import terrain_triangles

MAXIMUM_GRADE = 0.08
MINIMUM_END_CROSSFALL_REACH_M = 6.0


@dataclass(frozen=True)
class PavementProfile:
    points: np.ndarray
    triangles: np.ndarray
    sections: np.ndarray
    offsets: np.ndarray
    prior: np.ndarray

    def offset_bounds(self):
        low, high = np.zeros(len(self.prior)), np.zeros(len(self.prior))
        np.minimum.at(low, self.sections, self.offsets)
        np.maximum.at(high, self.sections, self.offsets)
        return low, high

    def longitudinal_grade_terms(self, route):
        ids = self.sections[self.triangles]
        panels = ids.min(axis=1)
        centres = route.sections.mean(axis=1)
        forward = np.diff(centres, axis=0)
        forward /= np.linalg.norm(forward, axis=1)[:, None]
        xy = self.points[self.triangles]
        edges = xy[:, 1:] - xy[:, :1]
        projection = np.linalg.solve(
            edges.transpose(0, 2, 1), forward[panels, :, None]
        )[..., 0]
        station_delta = ids[:, 1:] - ids[:, :1]
        offsets = self.offsets[self.triangles]
        gain = (projection * station_delta).sum(axis=1)
        bias = (projection * (offsets[:, 1:] - offsets[:, :1])).sum(axis=1)
        return panels, gain, bias


class GroundSections:
    def __init__(self, vertices, quads, terrain, junctions):
        self.terrain = terrain
        self.patches = [
            SurfaceSampler(j.mesh, xy_tolerance_m=2e-6, height_tolerance_m=3e-6)
            for j in junctions
        ]
        triangles = [vertices[terrain_triangles(quads), :2]]
        triangles.extend(j.mesh.vertices[j.mesh.triangles, :2] for j in junctions)
        borders = shapely.boundary(shapely.polygons(np.concatenate(triangles)))
        self.edges = shapely.STRtree(borders)

    def heights(self, points):
        heights = self.terrain(points)
        for patch in self.patches:
            z = patch.sample(points).points_enu_m[:, 2]
            found = np.isfinite(z)
            heights[found] = z[found]
        return heights

    def end_breaks(self, a, b):
        edge = shapely.LineString([a, b])
        borders = self.edges.geometries[self.edges.query(edge, predicate="intersects")]
        xy = shapely.get_coordinates(shapely.intersection(edge, borders))
        axis = b - a
        length = np.linalg.norm(axis)
        along = np.sort(((xy - a) @ axis) / length)
        along = along[(along > 2e-6) & (along < length - 2e-6)]
        keep = np.diff(along, prepend=-np.inf) > 2e-6
        return along[keep] / length

    def breaks_in(self, footprint):
        borders = self.edges.geometries[
            self.edges.query(footprint, predicate="intersects")
        ]
        parts = shapely.get_parts(shapely.intersection(footprint, borders))
        return parts[shapely.get_type_id(parts) == 1]


def profile_vertices(sections, fractions=(0.0, 0.5, 1.0)):
    cross = sections[:, 1] - sections[:, 0]
    return sections[:, :1] + cross[:, None, :] * np.asarray(fractions)[None, :, None]


def profile_triangles(count, width=3):
    cells = (np.arange(count - 1)[:, None] * width + np.arange(width - 1)).ravel()
    return (cells[:, None] + [0, 1, width + 1, 0, width + 1, width]).reshape(-1, 3)


def endpoint_crossfall(route, heights, centre):
    offsets = np.zeros_like(heights)
    stair_ends = {contact.end for contact in route.stair_contacts}
    for side, distance in enumerate((route.station, route.station[-1] - route.station)):
        if side in stair_ends:
            continue
        end = (0, -1)[side]
        edge_offsets = heights[end] - heights[end, centre]
        reach = min(
            route.station[-1],
            max(
                MINIMUM_END_CROSSFALL_REACH_M,
                3 * abs(edge_offsets).max() / MAXIMUM_GRADE,
            ),
        )
        blend = np.clip(1 - distance / reach, 0, 1)
        offsets += (blend * blend * (3 - 2 * blend))[:, None] * edge_offsets
    for contact in route.stair_contacts:
        offsets[contact.sections] = 0.0
    return offsets


def sample_profile(route, ground):
    fixed = np.array([0.0, 0.5, 1.0])
    stair_ends = {contact.end for contact in route.stair_contacts}
    extra = np.sort(
        np.concatenate(
            [
                np.empty(0),
                *(
                    ground.end_breaks(*section)
                    for side, section in enumerate(route.sections[[0, -1]])
                    if side not in stair_ends
                ),
            ]
        )
    )
    spacing = 2e-6
    extra = extra[np.min(abs(extra[:, None] - fixed), axis=1) > spacing]
    extra = extra[np.diff(extra, prepend=-np.inf) > spacing]
    fractions = np.sort(np.r_[fixed, extra])
    centre = np.searchsorted(fractions, 0.5)
    grid = profile_vertices(route.sections, fractions)
    count, width = grid.shape[:2]
    points = grid.reshape(-1, 2)
    heights = ground.heights(points).reshape(count, width)
    offsets = endpoint_crossfall(route, heights, centre)
    return PavementProfile(
        points,
        profile_triangles(count, width),
        np.repeat(np.arange(count), width),
        offsets.ravel(),
        heights[:, centre],
    )
