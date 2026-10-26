"""Connected leaf carrier with one unrolled opacity/normal map and shared seams."""

import hashlib
import json
from dataclasses import asdict

import numpy as np
import shapely
from leaf_sheet import average_blocks, organ_colour
from outshine_ref.vegetation.curve_frame import arc_frame
from outshine_ref.vegetation.normal_distribution import jensen_error_bound
from raster_batch import rasterize


def affine(points, triangle, positions):
    weights = np.linalg.solve(
        (triangle[1:] - triangle[0]).T, (points - triangle[0]).T
    ).T
    return np.column_stack((1 - weights.sum(axis=1), weights)) @ positions


def paired_surface_bound(source, source_xy, mesh, grid_xy):
    """Convex norm maxima at the vertices of every source/carrier triangle overlap."""
    original = source_xy[source["faces"]]
    carrier = grid_xy[mesh["faces"]]
    a, b = shapely.polygons(original), shapely.polygons(carrier)
    uncovered = float(shapely.area(shapely.difference(a, shapely.union_all(b))).sum())
    if uncovered > float(shapely.area(a).sum()) * 1e-12 + 1e-15:
        raise ValueError("carrier leaves original UV surface uncovered")
    maximum, area = 0.0, 0.0
    for i, triangle in enumerate(a):
        for j, target in enumerate(b):
            overlap = shapely.intersection(triangle, target)
            if shapely.area(overlap) <= 0:
                continue
            points = shapely.get_coordinates(overlap)
            left = affine(points, original[i], source["vertices"][source["faces"][i]])
            right = affine(points, carrier[j], mesh["vertices"][mesh["faces"][j]])
            maximum = max(maximum, float(np.linalg.norm(left - right, axis=1).max()))
            area += float(shapely.area(overlap))
    expected = float(shapely.area(a).sum())
    if not np.isclose(area, expected, rtol=1e-12, atol=1e-15):
        raise ValueError(
            "carrier must cover the complete original UV surface exactly once"
        )
    return maximum


def build_connected(blade, variant=0, *, size=32, segments=2):
    if (
        isinstance(variant, (bool, np.bool_))
        or not isinstance(variant, (int, np.integer))
        or not 0 <= variant < 64
    ):
        raise ValueError("bounded integer variant identity required")
    if type(size) is not int or size not in (16, 32, 64):
        raise ValueError("bounded whole-leaf map required")
    if type(segments) is not int or segments not in (2, 4):
        raise ValueError("two or four connected longitudinal intervals required")
    source = blade.mesh(8, 1)
    t, s = source["parameters"].T
    width = blade.width_profile(np.maximum(t, 0))[0]
    xy = np.column_stack((s * width, t * blade.length))
    lower, upper = xy.min(axis=0), xy.max(axis=0)
    padding = (upper - lower) * 2 / (size - 4)
    lower, upper = lower - padding, upper + padding
    parameters = np.linspace(float(t.min()), 1.0, segments + 1)
    tx, x = np.meshgrid(parameters, (lower[0], 0.0, upper[0]), indexing="ij")
    centre, _, lateral, normal = arc_frame(
        blade.length, blade.bend, blade.twist, np.maximum(tx, 0)
    )
    centre[..., 1] += np.minimum(tx, 0) * blade.length
    vertices = (
        centre + x[..., None] * lateral + (abs(x) * blade.fold)[..., None] * normal
    )
    rows = np.arange(vertices.size // 3).reshape(-1, 3)
    a, b = rows[:-1, :-1], rows[:-1, 1:]
    d, c = rows[1:, :-1], rows[1:, 1:]
    faces = (
        np.stack((np.stack((a, b, c), axis=-1), np.stack((a, c, d), axis=-1)), axis=2)
        .reshape(-1, 3)
        .astype(np.int32)
    )
    grid_xy = np.column_stack((x.ravel(), tx.ravel() * blade.length))
    triangle = vertices.reshape(-1, 3)[faces]
    n = np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0])
    lengths = np.linalg.norm(n, axis=1)
    if np.any(lengths <= 0):
        raise ValueError("nondegenerate connected carrier required")
    mesh = {
        "vertices": vertices.reshape(-1, 3),
        "faces": faces,
        "normals": np.repeat(n / lengths[:, None], 3, axis=0),
        "uv": (grid_xy - lower) / (upper - lower),
    }
    attributes = np.concatenate(
        (source["normals"].reshape(-1, 3, 3), source["parameters"][source["faces"]]),
        axis=-1,
    )
    mask, samples = rasterize(
        np.column_stack((xy, np.zeros(len(xy)))),
        source["faces"],
        attributes,
        lower,
        upper,
        2 * size,
    )
    normals = samples[..., :3].astype(float)
    normals /= np.maximum(np.linalg.norm(normals, axis=-1, keepdims=True), 1e-12)
    alpha = mask[..., None].astype(float)
    colour = average_blocks(
        np.concatenate((organ_colour(variant, samples[..., 4]) * alpha, alpha), axis=-1)
    )
    normal_map = average_blocks(
        np.concatenate(((normals + 1) / 2 * alpha, alpha), axis=-1)
    )
    mass = normal_map[..., 3].ravel()
    vectors = 2 * normal_map[..., :3].reshape(-1, 3) - mass[:, None]
    field = np.column_stack((mass, vectors))[:, None, :]
    recipe = {
        "format": "connected-leaf-carrier-1",
        "blade": asdict(blade),
        "variant": int(variant),
        "map_size": size,
        "longitudinal_intervals": segments,
        "source_mesh": [8, 1],
        "normal_encoding": "blade-object-frame-premultiplied-raw-first-moment",
    }
    result = {
        "mesh": mesh,
        "colour": colour,
        "normal": normal_map,
        "source_triangles": len(source["faces"]),
        "paired_source_surface_bound_m": paired_surface_bound(
            source, xy, mesh, grid_xy
        ),
        "native_normal_response_bound": float(jensen_error_bound(field).max()),
        "identity": hashlib.sha256(
            json.dumps(recipe, sort_keys=True).encode()
        ).hexdigest(),
        "recipe": recipe,
    }
    for v in (*mesh.values(), colour, normal_map):
        v.setflags(write=False)
    return result
