"""A bounded inferred retaining strip between independently supplied interfaces.

The source-water line, dry rim and actual road interface are caller contracts.
End caps close this local solid; they are continuation ports, not physical banks.
"""

import numpy as np
import shapely
from pool_body import positive_faces


def contact_edges(body):
    if "contact_edges" in body:
        return body["contact_edges"]
    return np.vstack(
        [
            np.column_stack((chain[:-1], chain[1:]))
            for chain in (body["shore_lower"], body["road_upper"])
        ]
    )


def combine(bodies):
    """One owner for separate closed bank components; no connecting gap edges."""
    if not 1 <= len(bodies) <= 8:
        raise ValueError("bounded supplied bank components required")
    vertices, faces, roofs, contacts, sources = [], [], [], [], []
    offset = 0
    for index, body in enumerate(bodies):
        vertices.append(body["vertices"])
        faces.append(body["faces"] + offset)
        roofs.append(body["roof_faces"] + offset)
        contacts.append(contact_edges(body) + offset)
        sources.append(np.full(len(body["faces"]), index, dtype="<u4"))
        offset += len(body["vertices"])
    result = {
        "vertices": np.vstack(vertices),
        "faces": np.vstack(faces),
        "roof_faces": np.vstack(roofs),
        "contact_edges": np.vstack(contacts),
        "source_components": np.concatenate(sources),
    }
    polygons = shapely.polygons(result["vertices"][result["roof_faces"], :2])
    if not shapely.coverage_is_valid(polygons):
        raise ValueError(
            "bank components overlap or have nonconforming plan boundaries"
        )
    return result


def build(shore_water, dry_rim_up, road_interface, *, bed_depth_m):
    shore = np.asarray(shore_water, float)
    road = np.asarray(road_interface, float)
    rim = np.asarray(dry_rim_up, float)
    if any(
        p.ndim != 2 or p.shape[1] != 3 or not 2 <= len(p) <= 1024 for p in (shore, road)
    ):
        raise ValueError("bounded ordered shore and road XYZ interfaces required")
    if (
        rim.shape != (len(shore),)
        or not all(np.isfinite(p).all() for p in (shore, road, rim))
        or not np.isfinite(bed_depth_m)
        or not 0 < bed_depth_m <= 10
    ):
        raise ValueError("finite rim and explicit bounded bed depth required")
    xy = np.vstack((shore[:, :2], road[::-1, :2]))
    polygon = shapely.Polygon(xy)
    if (
        not polygon.is_valid
        or polygon.area <= 0
        or len(np.unique(xy, axis=0)) != len(xy)
    ):
        raise ValueError("simple noncollapsed interface strip required")
    # The lower road-side closure is an explicit procedural continuation of the
    # shore's bed level, not a sampled or reconstructed exterior water surface.
    line = shapely.LineString(shore[:, :2])
    stations = np.r_[
        0, np.cumsum(np.linalg.norm(np.diff(shore[:, :2], axis=0), axis=1))
    ]
    if np.any(np.diff(stations) <= 0) or not line.is_simple:
        raise ValueError("simple shore line with positive segment lengths required")
    feet = shapely.line_locate_point(line, shapely.points(road[:, :2]))
    bottom_up = np.r_[shore[:, 2], np.interp(feet, stations, shore[:, 2])[::-1]]
    bottom_up -= bed_depth_m
    top_up = np.r_[rim, road[::-1, 2]]
    if np.any(top_up <= bottom_up):
        raise ValueError("positive strip thickness required at every interface node")
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(polygon))
    coordinates = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    lookup = {tuple(point): i for i, point in enumerate(xy)}
    faces = positive_faces(
        xy,
        np.array([[lookup[tuple(p)] for p in face] for face in coordinates], np.int64),
    )
    projected = shapely.polygons(xy[faces])
    if not shapely.coverage_is_valid(projected) or not shapely.coverage_union_all(
        projected
    ).equals(polygon):
        raise ValueError("conforming complete strip triangulation required")
    n = len(xy)
    boundary = np.column_stack((np.arange(n), np.roll(np.arange(n), -1)))
    edges, counts = np.unique(
        np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1),
        axis=0,
        return_counts=True,
    )
    if set(map(tuple, edges[counts == 1])) != set(
        map(tuple, np.sort(boundary, axis=1))
    ):
        raise ValueError("triangulation lost an original interface segment")
    a, b = boundary.T
    walls = np.vstack((np.c_[a, b, n + b], np.c_[a, n + b, n + a]))
    # Polygon winding governs wall normals; top/bottom already have fixed winding.
    if not polygon.exterior.is_ccw:
        walls = walls[:, ::-1]
    vertices = np.vstack((np.c_[xy, bottom_up], np.c_[xy, top_up]))
    solid = np.vstack((faces[:, ::-1], faces + n, walls))
    oriented = solid[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    undirected, counts = np.unique(
        np.sort(oriented, axis=1), axis=0, return_counts=True
    )
    if np.any(counts != 2) or len(np.unique(oriented, axis=0)) != 2 * len(undirected):
        raise ValueError("closed consistently oriented indexed strip required")
    triangles = vertices[solid]
    volume = (
        np.einsum(
            "ij,ij->i", triangles[:, 0], np.cross(triangles[:, 1], triangles[:, 2])
        ).sum()
        / 6
    )
    if not np.isfinite(volume) or volume <= 0:
        raise ValueError("positive finite inferred strip volume required")
    return {
        "vertices": vertices,
        "faces": solid,
        "roof_faces": faces + n,
        "shore_lower": np.arange(len(shore)),
        "shore_upper": np.arange(len(shore)) + n,
        "road_upper": np.arange(len(shore), n)[::-1] + n,
        "continuation_edges": boundary[[len(shore) - 1, n - 1]],
    }
