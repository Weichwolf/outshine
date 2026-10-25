"""Closed indexed prisms with holes; strict local Float32 geometry qualification."""

from itertools import pairwise

import numpy as np
import shapely


def qualify(positions, indices, expected_euler):
    triangle = positions.astype(float)[indices]
    normal = np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0])
    if np.any(np.linalg.norm(normal, axis=1) == 0):
        raise ValueError("mass prism contains a zero-area triangle")
    directed = indices[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    edges = np.sort(directed, axis=1)
    unique, inverse, counts = np.unique(
        edges, axis=0, return_inverse=True, return_counts=True
    )
    orientation = np.where(directed[:, 0] < directed[:, 1], 1, -1)
    balance = np.bincount(inverse, weights=orientation)
    euler = len(positions) - len(unique) + len(indices)
    if np.any(counts != 2) or np.any(balance != 0) or euler != expected_euler:
        raise ValueError("mass prism is not a closed oriented indexed surface")
    volume = (
        np.einsum(
            "ij,ij->i", triangle[:, 0], np.cross(triangle[:, 1], triangle[:, 2])
        ).sum()
        / 6
    )
    if volume <= 0:
        raise ValueError("mass prism has no positive oriented volume")
    return normal, float(volume), int(euler)


def extrude(domain, heights):
    low, high = heights
    if (
        not domain.is_valid
        or domain.is_empty
        or not np.isfinite(heights).all()
        or high <= low
    ):
        raise ValueError("valid finite polygon and positive prism height required")
    domain = shapely.orient_polygons(domain)
    if shapely.get_num_coordinates(domain) > 16384:
        raise ValueError("bounded aggregate prism contour count required")
    parts = shapely.get_parts(domain)
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(domain))
    if not len(triangles) or not shapely.union_all(triangles).equals(domain):
        raise ValueError("prism caps must cover the complete footprint")
    xy = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    a, b = xy[:, 1] - xy[:, 0], xy[:, 2] - xy[:, 0]
    cap = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    xy[cap < 0] = xy[cap < 0, ::-1]
    top = np.dstack((xy, np.full(xy.shape[:2], high)))
    bottom = np.dstack((xy[:, ::-1], np.full(xy.shape[:2], low)))
    walls = []
    holes = 0
    for polygon in parts:
        holes += len(polygon.interiors)
        for ring in (polygon.exterior, *polygon.interiors):
            coordinates = shapely.get_coordinates(ring)
            for a, b in pairwise(coordinates):
                al, bl, ah, bh = (
                    np.r_[p, z] for p, z in ((a, low), (b, low), (a, high), (b, high))
                )
                walls.extend(((al, bl, bh), (al, bh, ah)))
    coordinates = np.concatenate((top, bottom, walls))
    if len(coordinates) > 32768:
        raise ValueError("bounded aggregate prism triangle count required")
    positions, inverse = np.unique(
        coordinates.reshape(-1, 3), axis=0, return_inverse=True
    )
    indices = inverse.reshape(-1, 3)
    normal, volume, euler = qualify(positions, indices, 2 * (len(parts) - holes))
    packed = positions.astype(np.float32)
    if len(np.unique(packed.view(np.uint32), axis=0)) != len(packed):
        raise ValueError("distinct mass prism nodes collapse in Float32")
    packed_normal, packed_volume, _ = qualify(packed, indices, euler)
    if np.any(np.einsum("ij,ij->i", normal, packed_normal) <= 0):
        raise ValueError("mass prism face orientation changes in Float32")
    if len(packed) > 65536:
        raise ValueError("bounded UInt16 mass prism required")
    return {"positions": packed, "indices": indices.astype(np.uint16)}, {
        "vertices": len(packed),
        "triangles": len(indices),
        "euler": euler,
        "double_volume_m3": volume,
        "packed_volume_m3": packed_volume,
        "maximum_float32_vertex_rounding_m": float(
            np.linalg.norm(packed - positions, axis=1).max()
        ),
        "cap_xy_bytes": xy.nbytes,
        "triangle_coordinate_bytes": coordinates.nbytes,
        "unique_double_position_bytes": positions.nbytes,
    }
