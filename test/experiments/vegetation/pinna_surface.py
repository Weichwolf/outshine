"""Own cached leaf-plane alternatives; GEOS handles union, clipping and triangles."""

import numpy as np
import shapely
from outshine_ref.geometry import conform_triangles


def leaf_source(mesh):
    faces = mesh["faces"][mesh["surface"] == 6]
    original, indices = np.unique(faces, return_inverse=True)
    return mesh["vertices"][original], indices.reshape(-1, 3).astype(np.int32)


def domain(vertices, faces):
    if not 0 < len(faces) <= 8192 or not np.isfinite(vertices).all():
        raise ValueError("bounded finite leaf source required")
    p = vertices[faces, :2]
    det = (p[:, 1, 0] - p[:, 0, 0]) * (p[:, 2, 1] - p[:, 0, 1]) - (
        p[:, 1, 1] - p[:, 0, 1]
    ) * (p[:, 2, 0] - p[:, 0, 0])
    if np.any(abs(det) < 1e-14):
        raise ValueError("nondegenerate source XY projection required")
    return shapely.union_all(shapely.polygons(p))


def surface(mesh, method="quad", across=4, along=8, simplify_m=0.003):
    if (
        method not in ("quad", "contour")
        or type(across) is not int
        or not 1 <= across <= 16
        or type(along) is not int
        or not 1 <= along <= 32
        or not np.isfinite(simplify_m)
        or not 0 <= simplify_m <= 0.01
    ):
        raise ValueError("declared bounded plane grid required")
    vertices, faces = leaf_source(mesh)
    region = domain(vertices, faces)
    contour = region
    if method == "contour" and simplify_m:
        contour = region.simplify(simplify_m, preserve_topology=True).buffer(
            np.sqrt(2) * simplify_m, quad_segs=1
        )
        if region.difference(contour).area > 1e-12:
            raise ValueError("simplified contour does not cover the full source domain")
    lower, upper = (
        vertices[:, :2].min(axis=0) - 0.015,
        vertices[:, :2].max(axis=0) + 0.015,
    )
    lower[1], upper[1] = max(0.0, lower[1]), min(1.0, upper[1])
    z = (vertices[:, 2].min() + vertices[:, 2].max()) / 2
    x, y = (
        np.linspace(lower[0], upper[0], across + 1),
        np.linspace(lower[1], upper[1], along + 1),
    )
    if method == "quad":
        xx, yy = np.meshgrid(x, y)
        xy = np.c_[xx.ravel(), yy.ravel()]
        a = (np.arange(along)[:, None] * (across + 1) + np.arange(across)).ravel()
        f = np.vstack(
            (np.c_[a, a + 1, a + across + 2], np.c_[a, a + across + 2, a + across + 1])
        ).astype(np.int32)
        report = {"split_edges": 0, "added_triangles": 0}
    else:
        cells = shapely.box(
            np.tile(x[:-1], along),
            np.repeat(y[:-1], across),
            np.tile(x[1:], along),
            np.repeat(y[1:], across),
        )
        pieces = shapely.intersection(cells, contour)
        triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(pieces))
        if len(triangles) > 8192:
            raise ValueError("contour triangle budget")
        raw = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
        xy, index = np.unique(raw.reshape(-1, 2), axis=0, return_inverse=True)
        f = index.reshape(-1, 3)
        p = xy[f]
        det = np.cross(
            np.c_[p[:, 1] - p[:, 0], np.zeros(len(p))],
            np.c_[p[:, 2] - p[:, 0], np.zeros(len(p))],
        )[:, 2]
        f[det < 0] = f[det < 0, ::-1]
        f, report = conform_triangles(xy, f, max_faces=8192, max_candidates=1_000_000)
        f = f.astype(np.int32)
    uv = (xy - lower) / (upper - lower)
    result = {
        "vertices": np.c_[xy, np.full(len(xy), z)],
        "faces": f,
        "normals": np.tile((0.0, 0.0, 1.0), (f.size, 1)),
        "corner_uv": uv[f.ravel()],
        "organ_ids": np.full((len(xy), 1), np.iinfo(np.uint32).max - 1, np.uint32),
        "surface": np.full(len(f), 7, np.uint8),
    }
    return result, {
        "method": method,
        "grid": [across, along],
        "lower": lower.tolist(),
        "upper": upper.tolist(),
        "height": float(z),
        "source_depth_half": float(np.ptp(vertices[:, 2]) / 2),
        "source_leaf_faces": len(faces),
        "source_projected_area": float(region.area),
        "rectangle_area": float(np.prod(upper - lower)),
        "contour_area": float(contour.area),
        "source_missing_area": float(region.difference(contour).area),
        "simplify_m": simplify_m if method == "contour" else 0,
        "conformance": report,
    }


def weights(points, triangles):
    a, b = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    q = points - triangles[:, 0]
    det = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    u = (q[:, 0] * b[:, 1] - q[:, 1] * b[:, 0]) / det
    v = (a[:, 0] * q[:, 1] - a[:, 1] * q[:, 0]) / det
    return np.c_[1 - u - v, u, v]


def overlay(source, coarse, max_pairs=65536):
    """Overlay vertices bound the difference of both piecewise-affine surfaces."""
    vertices, faces = leaf_source(source)
    fine = vertices[faces, :2]
    projected = coarse["vertices"][coarse["faces"], :2]
    original, proxy = shapely.polygons(fine), shapely.polygons(projected)
    pairs = shapely.STRtree(proxy).query(original, predicate="intersects")
    if len(pairs[0]) > max_pairs:
        raise ValueError("overlay pair budget")
    regions = shapely.intersection(original[pairs[0]], proxy[pairs[1]])
    valid = shapely.area(regions) > 1e-15
    regions = regions[valid]
    pairs = pairs[:, valid]
    coordinates, indices = shapely.get_coordinates(regions, return_index=True)
    if len(coordinates) > 262144:
        raise ValueError("overlay point budget")
    si, ci = pairs[:, indices]
    ws, wc = weights(coordinates, fine[si]), weights(coordinates, projected[ci])
    return {
        "source_triangles": faces[si],
        "coarse_triangles": coarse["faces"][ci],
        "source_weights": ws,
        "coarse_weights": wc,
        "source_vertex_indices": np.unique(source["faces"][source["surface"] == 6]),
        "regions": len(regions),
        "pairs": len(pairs[0]),
        "points": len(coordinates),
    }
