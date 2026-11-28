"""Project the solved terrain into wall coordinates for opening clearance."""

import numpy as np
import shapely

from outshine.terrain.ground_surface import ROUTE_MATERIALS, SOIL_MATERIALS

# The ribbon includes both the recess behind the wall and the projecting sill.
OPENING_DEPTH_M = 0.18
GROUND_GAP_M = 0.012


class WallGround:
    def __init__(self, surfaces):
        ground = [
            np.asarray(points)[np.asarray(faces)]
            for name, (points, faces) in surfaces.items()
            if name in (*SOIL_MATERIALS, *ROUTE_MATERIALS) and len(faces)
        ]
        xyz = np.concatenate(ground) if ground else np.empty((0, 3, 3))
        normals = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        valid = np.abs(normals[:, 2]) > 1e-10
        self.origins = xyz[valid, 0]
        self.maximum = xyz[valid, :, 2].max(axis=1) + GROUND_GAP_M
        self.slopes = -normals[valid, :2] / normals[valid, 2, None]
        self.index = shapely.STRtree(shapely.polygons(xyz[valid, :, :2]))

    def buried(self, a, b, bottom, top):
        length = np.linalg.norm(b - a)
        if length < 0.01 or not len(self.index):
            return None
        axis = (b - a) / length
        across = axis[::-1] * [1, -1] * OPENING_DEPTH_M
        strip = shapely.Polygon([a + across, b + across, b - across, a - across])
        hits = self.index.query(strip, "intersects")
        hits = hits[self.maximum[hits] > bottom]
        if not len(hits):
            return None
        pieces = shapely.intersection(self.index.geometries[hits], strip)
        xy, owners = shapely.get_coordinates(pieces, return_index=True)
        if not len(owners):
            return None
        parents = hits[owners]
        origins = self.origins[parents]
        heights = origins[:, 2] + np.einsum(
            "ij,ij->i", xy - origins[:, :2], self.slopes[parents]
        )
        u = np.clip((xy - a) @ axis, 0, length)
        profile = np.column_stack((u, heights + GROUND_GAP_M))
        cuts = []
        for points in np.split(profile, np.flatnonzero(np.diff(owners)) + 1):
            if points[:, 1].max() <= bottom:
                continue
            start, stop = points[:, 0].min(), points[:, 0].max()
            if stop - start < 1e-8:
                continue
            support = np.vstack((points, [start, bottom - 1], [stop, bottom - 1]))
            cuts.append(shapely.MultiPoint(support).convex_hull)
        if not cuts:
            return None
        return shapely.union_all(cuts).intersection(shapely.box(0, bottom, length, top))
