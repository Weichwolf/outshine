"""Bind every retained free region to exactly one original plane face."""

import numpy as np
import shapely


def source_faces(geometry, vertices, triangles, *, expected_holes, prefix):
    points = np.asarray(geometry["source_nodes_EN"])
    edges = np.asarray(geometry["source_edges_local"])
    faces = shapely.polygonize(shapely.linestrings(points[edges]))
    if len(faces.geoms) != expected_holes:
        raise ValueError(prefix + "_original_plane_faces_required")
    domain = shapely.coverage_union_all(shapely.polygons(vertices[triangles, :2]))
    if (
        domain.geom_type != "Polygon"
        or not domain.is_valid
        or len(domain.interiors) != expected_holes
    ):
        raise ValueError(prefix + "_hole_topology")
    holes = [shapely.Polygon(ring) for ring in domain.interiors]
    assignment = np.array(
        [[face.covers(hole) for hole in holes] for face in faces.geoms]
    )
    if not np.all(assignment.sum(axis=0) == 1) or not np.all(
        assignment.sum(axis=1) == 1
    ):
        raise ValueError(prefix + "_source_holes_not_preserved")
    return tuple(face.wkb_hex for face in faces.geoms)
