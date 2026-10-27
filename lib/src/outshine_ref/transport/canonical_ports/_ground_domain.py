"""Triangulate exact shared polygon parts without welding distinct source nodes."""

import numpy as np
import shapely


def mesh_parts(polygons, frames, roles, ports, links, caps, *, prefix="ground_network"):
    if any(p.geom_type != "Polygon" or not p.is_valid or p.area <= 0 for p in polygons):
        raise ValueError(prefix + "_invalid_polygon")
    if not shapely.coverage_is_valid(polygons):
        raise ValueError(prefix + "_overlap_or_nonconforming_parts")
    complete = shapely.coverage_union_all(polygons)
    if complete.geom_type != "Polygon" or not complete.is_valid:
        raise ValueError(prefix + "_disconnected_domain")
    if complete.interiors:
        raise ValueError(prefix + "_unexpected_hole")
    triangles, owners = [], []
    for part, polygon in enumerate(polygons):
        tri = shapely.get_coordinates(
            shapely.constrained_delaunay_triangles(polygon)
        ).reshape(-1, 4, 2)[:, :3]
        triangles.extend(tri)
        owners.extend([part] * len(tri))
    vertices, indices = np.unique(
        np.array(triangles).reshape(-1, 2), axis=0, return_inverse=True
    )
    faces, owners = indices.reshape(-1, 3), np.asarray(owners)
    lookup = {tuple(p): i for i, p in enumerate(vertices)}

    def indexed(sections):
        return np.asarray(
            [[lookup[tuple(p)] for p in section] for section in sections],
            dtype=np.int64,
        ).reshape(-1, 2)

    return {
        "xy": vertices,
        "faces": faces,
        "owners": owners,
        "roles": np.asarray(roles),
        "ports": indexed(ports),
        "links": np.asarray(links, dtype=np.int64).reshape(-1, 2),
        "caps": indexed(caps),
        "frames": np.asarray(frames)[owners],
    }, {
        "parts": len(polygons),
        "prior_vertices": len(vertices),
        "prior_faces": len(faces),
        "whole_ports": len(ports),
        "domain_area_m2": complete.area,
    }
