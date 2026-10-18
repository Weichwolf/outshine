"""Finite GGX slope-kernel mass; the full native BRDF remains a residual."""

import itertools

import numpy as np
from facade_footprint import screen_polygon
from masonry_relief_reference import pieces
from relief_shading_reference import normals, specular_from_dots
from shapely.geometry import Polygon, box
from shapely.geometry.polygon import orient


def split_polygon(shape, tiles):
    if tiles == 1:
        return [shape]
    if tiles != 4:
        raise ValueError("only one or four finite residual tiles are declared")
    x0, y0, x1, y1 = shape.bounds
    xs, ys = (x0, (x0 + x1) / 2, x1), (y0, (y0 + y1) / 2, y1)
    return [
        clipped
        for i, j in itertools.product(range(2), repeat=2)
        if (clipped := shape.intersection(box(xs[i], ys[j], xs[i + 1], ys[j + 1]))).area
        > 1e-18
    ]


def footprint(case, tiles):
    """Exact ideal geometry, not a bounded or cached runtime statistic."""
    at, dx, dy, depth = (case[k] for k in ("at", "dx", "dy", "depth"))
    constants, corners = {}, []
    work = {
        "bricks": 0,
        "clip_attempts": 0,
        "corner_pieces": 0,
        "edges": 0,
        "constant_normals": 1,
        "finite_tiles": 0,
        "retained_array_bytes": 24,
    }
    if depth == 0:
        return np.zeros((1, 2)), np.ones(1), corners, work
    bevel = 0.002
    extent = (np.abs(dx) + np.abs(dy)) / 2
    low, high = at - extent - bevel / 2, at + extent + bevel / 2
    covered = 0.0
    for row in range(
        int(np.floor(low[1] / 0.08)) - 1, int(np.ceil(high[1] / 0.08)) + 1
    ):
        shift = (row % 2) * 0.125
        first = int(np.floor((low[0] - shift) / 0.25)) - 1
        last = int(np.ceil((high[0] - shift) / 0.25)) + 1
        for column in range(first, last + 1):
            x, y = column * 0.25 + shift, row * 0.08
            work["bricks"] += 1
            for horizontal, vertical in itertools.product(
                pieces(x + 0.005, x + 0.245, bevel),
                pieces(y + 0.005, y + 0.075, bevel),
            ):
                if horizontal[2] == vertical[2] == 0:
                    continue
                work["clip_attempts"] += 1
                shape = screen_polygon(
                    at,
                    dx,
                    dy,
                    [horizontal[0], vertical[0]],
                    [horizontal[1], vertical[1]],
                )
                if shape.area < 1e-18:
                    continue
                covered += shape.area
                points = np.asarray(shape.exterior.coords[:-1])
                material = at + points[:, :1] * dx + points[:, 1:] * dy
                u = horizontal[2] * (material[:, 0] - horizontal[0]) + horizontal[3]
                v = vertical[2] * (material[:, 1] - vertical[0]) + vertical[3]
                gradient = depth * np.column_stack([horizontal[2] * v, vertical[2] * u])
                if horizontal[2] == 0 or vertical[2] == 0:
                    key = tuple(gradient[0])
                    constants[key] = constants.get(key, 0) + shape.area
                    continue
                slope = Polygon(gradient)
                if slope.area <= 1e-18:
                    raise ValueError(
                        "rank-degenerate varying slope needs a 1D integral"
                    )
                work["corner_pieces"] += 1
                density = shape.area / slope.area
                for tile in split_polygon(slope, tiles):
                    vertices = np.asarray(orient(tile, sign=1).exterior.coords[:-1])
                    corners.append((vertices, density))
                    work["edges"] += len(vertices)
    if covered > 1 + 1e-10:
        raise ValueError("overlapping bevel pieces")
    constants[(0.0, 0.0)] = max(1 - covered, 0)
    gradient, coverage = np.array(list(constants)), np.array(list(constants.values()))
    total = coverage.sum() + sum(Polygon(p).area * density for p, density in corners)
    np.testing.assert_allclose(total, 1, rtol=0, atol=1e-11)
    work["constant_normals"] = len(coverage)
    work["finite_tiles"] = len(corners)
    work["retained_array_bytes"] = (
        gradient.nbytes + coverage.nbytes + sum(p.nbytes + 8 for p, _ in corners)
    )
    return gradient, coverage, corners, work


def transform(roughness, lights, view):
    half = lights + view
    half /= np.linalg.norm(half, axis=1)[:, None]
    a2 = roughness**4
    xy, z = half[:, :2], half[:, 2]
    determinant = z * z + a2 * (1 - z * z)
    matrix = np.eye(2)[None] + (a2 - 1) * xy[:, :, None] * xy[:, None, :]
    linear = (1 - a2) * z[:, None] * xy
    centre = -np.linalg.solve(matrix, linear[..., None])[..., 0]
    delta = a2 / determinant  # Schur complement, avoiding cancellation.
    cholesky = np.linalg.cholesky(matrix)
    inverse = np.linalg.inv(cholesky.swapaxes(1, 2)) * np.sqrt(delta)[:, None, None]
    forward = cholesky.swapaxes(1, 2) / np.sqrt(delta)[:, None, None]
    return centre, forward, inverse, np.sqrt(determinant) / np.pi, half


def kernel_moments(vertices, transformed):
    """Positive CCW convex polygon; cosine boundary mass and first moment."""
    centre, forward, inverse, factor, _ = transformed
    a = np.einsum("lij,lvj->lvi", forward, vertices[None] - centre[:, None])
    b = np.roll(a, -1, axis=1)
    edge = b - a
    cross = a[..., 0] * b[..., 1] - a[..., 1] * b[..., 0]
    distance = np.sqrt(np.sum(edge * edge, axis=2) + cross * cross)
    line = np.arctan2(distance, 1 + np.sum(a * b, axis=2)) / distance
    mass = 0.5 * np.sum(cross * line, axis=1)
    first = -0.5 * np.sum(
        np.stack([edge[..., 1], -edge[..., 0]], axis=2) * line[..., None], axis=1
    )
    if not np.all(np.isfinite(mass)) or np.any(mass <= 0):
        raise ValueError("nonpositive or invalid analytic kernel mass")
    centroid = centre + np.einsum("lij,lj->li", inverse, first / mass[:, None])
    edge = np.roll(vertices, -1, axis=0) - vertices
    offset = centroid[:, None] - vertices[None]
    side = edge[None, :, 0] * offset[..., 1] - edge[None, :, 1] * offset[..., 0]
    if np.any(side < -1e-9):
        raise ValueError("analytic kernel centroid lies outside its polygon")
    return factor * mass, centroid


def point_response(gradient, roughness, lights, view):
    unit = normals(gradient)
    half = lights + view
    half /= np.linalg.norm(half, axis=1)[:, None]
    return specular_from_dots(
        roughness,
        np.maximum(unit @ lights.T, 0),
        np.maximum(unit @ view, 1e-6)[:, None],
        np.maximum(unit @ half.T, 0),
        np.maximum(half @ view, 0)[None],
    )


def finite_response(model, roughness, lights, view):
    gradient, coverage, corners, _ = model
    result = coverage @ point_response(gradient, roughness, lights, view)
    if not corners:
        return result
    transformed = transform(roughness, lights, view)
    half = transformed[-1]
    for vertices, density in corners:
        mass, centroid = kernel_moments(vertices, transformed)
        unit = normals(centroid)
        nl = np.maximum(np.sum(unit * lights, axis=1), 0)
        nv = np.maximum(unit @ view, 1e-6)
        nh = np.maximum(np.sum(unit * half, axis=1), 0)
        response = specular_from_dots(roughness, nl, nv, nh, np.maximum(half @ view, 0))
        q = (
            1
            + np.sum(centroid * centroid, axis=1)
            + (roughness**4 - 1)
            * (half[:, 2] - np.sum(half[:, :2] * centroid, axis=1)) ** 2
        )
        kernel = roughness**4 / (np.pi * q * q)
        result += density * mass * response / kernel
    return result
