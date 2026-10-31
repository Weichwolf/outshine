"""Finite geometry interfaces from the current shore/road capture, without nav inference."""

import gzip
import json

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.sampling import SurfaceSampler
from outshine_ref.places.height import decode_terrarium
from outshine_ref.water.geodetic import local_points
from shapely.ops import substring


def grid_faces(rows, columns):
    a = np.arange(rows * columns).reshape(rows, columns)[:-1, :-1].ravel()
    return np.vstack(
        (np.c_[a, a + columns, a + columns + 1], np.c_[a, a + columns + 1, a + 1])
    )


def selected_faces(triangles, bounds):
    return np.all(triangles[:, :, :2].max(axis=1) >= bounds[:2], axis=1) & np.all(
        triangles[:, :, :2].min(axis=1) <= bounds[2:], axis=1
    )


def roads(root, anchor, bounds):
    data = json.loads(gzip.decompress((root / "roads.json.gz").read_bytes()))
    if not np.array_equal(anchor, data["anchor"]):
        raise ValueError("road/water frames require explicit conversion")
    triangles, identities = [], []
    for part in data["parts"]:
        vertices = np.array(part["positions"]).reshape(-1, 3)
        placement = np.array(part["placement"]).reshape(4, 4, order="F")
        if not np.array_equal(placement[3], [0, 0, 0, 1]):
            raise ValueError("captured affine column-major placement required")
        vertices = vertices @ placement[:3, :3].T + placement[:3, 3]
        vertices = vertices[:, [0, 2, 1]] * [1, -1, 1]
        t = vertices[np.array(part["triangles"]).reshape(-1, 3)]
        chosen = np.flatnonzero(selected_faces(t, bounds))
        triangles.extend(t[chosen])
        identities.extend((part["part"], int(i)) for i in chosen)
    triangles = np.array(triangles)
    area = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )[:, 2]
    keep = area != 0
    return triangles[keep], np.array(identities)[keep], data["plan_key"]


def sample_roads(triangles, points):
    polygons = shapely.polygons(triangles[:, :, :2])
    pairs = shapely.STRtree(polygons).query(
        shapely.points(points), predicate="dwithin", distance=1e-8
    )
    result, bindings = [], []
    for i, point in enumerate(points):
        ids = pairs[1, pairs[0] == i]
        if not len(ids):
            raise ValueError("actual road boundary height is missing")
        a = triangles[ids, 0]
        u, v = triangles[ids, 1] - a, triangles[ids, 2] - a
        d = point - a[:, :2]
        denominator = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]
        b = (d[:, 0] * v[:, 1] - d[:, 1] * v[:, 0]) / denominator
        c = (u[:, 0] * d[:, 1] - u[:, 1] * d[:, 0]) / denominator
        z = a[:, 2] + b * u[:, 2] + c * v[:, 2]
        if not np.isfinite(z).all() or np.ptp(z) > 1e-7:
            raise ValueError("multiple road boundary heights need explicit ownership")
        result.append(z[0])
        bindings.append(ids.tolist())
    return np.column_stack((points, result)), bindings


def road_chain(shore, triangles):
    union = shapely.union_all(shapely.polygons(triangles[:, :, :2]))
    ends = shapely.points(np.array(shore.coords)[[0, -1]])
    ring = min(
        shapely.get_parts(union.boundary),
        key=lambda g: sum(g.distance(p) for p in ends),
    )
    feet = shapely.get_coordinates(shapely.shortest_line(ends, ring)).reshape(-1, 2, 2)[
        :, 1
    ]
    a, b = sorted(shapely.line_locate_point(ring, shapely.points(feet)))
    direct = substring(ring, a, b)
    wrap = shapely.LineString(
        np.vstack(
            (
                shapely.get_coordinates(substring(ring, b, ring.length)),
                shapely.get_coordinates(substring(ring, 0, a)),
            )
        )
    )
    chain = min((direct, wrap), key=lambda g: g.length)
    xy = shapely.get_coordinates(chain)
    if np.linalg.norm(xy[0] - np.array(shore.coords[0])) > np.linalg.norm(
        xy[-1] - np.array(shore.coords[0])
    ):
        xy = xy[::-1]
    return xy, union


def source_dem(root, anchor, manifest, bounds):
    tile = manifest["source_dem"]["tile"]
    heights = decode_terrarium(
        (root / f"dem-{tile[0]}-{tile[1]}-{tile[2]}.webp").read_bytes()
    )
    rows, columns = heights.shape
    x, y = np.meshgrid(
        tile[1] + np.arange(columns) / (columns - 1),
        tile[2] + np.arange(rows) / (rows - 1),
    )
    geo = np.c_[
        x.ravel() / 2 ** tile[0] * 360 - 180,
        np.rad2deg(np.arctan(np.sinh(np.pi * (1 - 2 * y.ravel() / 2 ** tile[0])))),
    ]
    vertices = local_points(anchor, geo, heights.ravel())
    faces = grid_faces(rows, columns)
    faces = faces[selected_faces(vertices[faces], bounds)]
    used, inverse = np.unique(faces, return_inverse=True)
    return SurfaceSampler(Mesh(vertices[used], inverse.reshape(-1, 3)))


def height(sampler, points):
    result = sampler.sample(points)
    if np.any(result.face_indices < 0):
        raise ValueError("original surface does not qualify every interface sample")
    return result.points_enu_m[:, 2]
