"""Queries and independent affine-surface volume integrals on actual roof triangles."""

import numpy as np
import shapely


class RoofSurface:
    def __init__(self, envelope):
        self.xyz = envelope.mesh["vertices"][
            envelope.mesh["faces"][envelope.surface_roles == 0]
        ]
        n = np.cross(self.xyz[:, 1] - self.xyz[:, 0], self.xyz[:, 2] - self.xyz[:, 0])
        if np.any(n[:, 2] <= 0):
            raise ValueError("Roof queries require an upward heightfield")
        self.gradient = -n[:, :2] / n[:, 2:3]
        self.polygons = shapely.polygons(self.xyz[:, :, :2])
        self.tree = shapely.STRtree(self.polygons)

    def height(self, xy):
        indices = self.tree.query(shapely.Point(xy), predicate="intersects")
        if not len(indices):
            raise ValueError("Roof query lies outside actual roof triangles")
        a = self.xyz[indices, 0]
        z = a[:, 2] + np.sum(
            (np.asarray(xy) - a[:, :2]) * self.gradient[indices], axis=1
        )
        if np.ptp(z) > 1e-7:
            raise ValueError("Overlapping roof triangles disagree on height")
        return float(z[0])

    def added_volume(self, polygon, cap):
        total, coverage = 0.0, []
        for index in self.tree.query(polygon, predicate="intersects"):
            region = polygon.intersection(self.polygons[index])
            if region.area == 0:
                continue
            coverage.append(region)
            a, gradient = self.xyz[index, 0], self.gradient[index]
            xy = shapely.get_coordinates(region)
            z = a[2] + (xy - a[:2]) @ gradient
            if z.min() >= cap:
                continue
            if z.max() > cap:
                axis = gradient / np.linalg.norm(gradient)
                tangent = np.array([-axis[1], axis[0]])
                u, v = (xy - a[:2]) @ axis, (xy - a[:2]) @ tangent
                edge = (cap - a[2]) / np.linalg.norm(gradient)
                clip = shapely.Polygon(
                    [
                        a[:2] + x * axis + y * tangent
                        for x, y in (
                            (u.min() - 1, v.min() - 1),
                            (edge, v.min() - 1),
                            (edge, v.max() + 1),
                            (u.min() - 1, v.max() + 1),
                        )
                    ]
                )
                region = region.intersection(clip)
            if region.area:
                centre = np.asarray(region.centroid.coords)[0]
                total += region.area * (cap - a[2] - (centre - a[:2]) @ gradient)
        if polygon.symmetric_difference(shapely.union_all(coverage)).area > 1e-8:
            raise ValueError("Dormer is not covered by the actual roof surface")
        return float(total)
