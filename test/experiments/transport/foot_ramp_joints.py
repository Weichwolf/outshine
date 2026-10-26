"""Metric coverage of a planned seam and both actual affine height meshes."""

import numpy as np
import shapely
from outshine_ref.geometry import SurfaceSampler
from retained_surface import surface_domain


def joint_error(left, right, line):
    points = [shapely.get_coordinates(line)]
    for mesh in [left, right]:
        if line.difference(surface_domain(mesh).boundary.buffer(1e-8)).length > 1e-10:
            raise ValueError(
                "whole ramp seam lacks actual boundary within metric XY tolerance"
            )
        faces = shapely.polygons(mesh.vertices[mesh.triangles, :2])
        points.append(shapely.get_coordinates(shapely.intersection(faces, line)))
    xy = np.unique(np.vstack(points), axis=0)
    if not 1 < len(xy) <= 65536:
        raise ValueError("bounded full ramp seam endpoints required")
    values = [
        SurfaceSampler(m, max_faces=262144).sample(xy, max_candidates=1048576)
        for m in [left, right]
    ]
    if any(any(n for n in value.reasons) for value in values):
        raise ValueError("actual ramp seam endpoint not on both selected caps")
    error = float(
        np.max(abs(values[0].points_enu_m[:, 2] - values[1].points_enu_m[:, 2]))
    )
    if error > 1e-7:
        raise ValueError("whole ramp seam has an actual height step")
    return error, len(xy)
