"""Conditional view moments per pixel/normal; no normal or lighting collapse."""

import numpy as np
import shapely
from area_measure_render import fragments
from packed_normals import unpack


def convex_moments(polygon, origin):
    """Exact degree-two area moments of a convex pixel fragment, centred stably."""
    mass, mean, covariance, triangles = batch_moments(
        [polygon], np.asarray(origin)[None]
    )
    return mass[0], mean[0], covariance[0], triangles


def batch_moments(polygons, origins):
    polygons = np.asarray(polygons, object)
    if np.any(shapely.get_type_id(polygons) != 3) or np.any(
        shapely.get_num_interior_rings(polygons)
    ):
        raise ValueError("convex polygon without holes required")
    vertices, owner = shapely.get_coordinates(polygons, return_index=True)
    sizes = shapely.get_num_coordinates(polygons)
    offsets = np.r_[0, np.cumsum(sizes)]
    at = np.arange(len(vertices))
    local = at - offsets[owner]
    middle = at[(local >= 1) & (local <= sizes[owner] - 3)]
    triangle_owner = owner[middle]
    anchors = vertices[offsets[:-1]]
    e = vertices[middle] - anchors[triangle_owner]
    f = vertices[middle + 1] - anchors[triangle_owner]
    cross = e[:, 0] * f[:, 1] - e[:, 1] * f[:, 0]
    positive = np.bincount(triangle_owner, weights=cross > 0, minlength=len(polygons))
    negative = np.bincount(triangle_owner, weights=cross < 0, minlength=len(polygons))
    if np.any((positive > 0) & (negative > 0)):
        raise ValueError("fragment fan changes winding")
    area = abs(cross) / 2
    mean = (e + f) / 3
    edges = np.stack((e, f, f - e), axis=1)
    covariance = np.einsum("nki,nkj->nij", edges, edges) / 36
    values = np.column_stack(
        (area, mean, covariance[:, 0, 0], covariance[:, 0, 1], covariance[:, 1, 1])
    )
    _, mass, centre, covariance = reduce_moments(triangle_owner, values)
    return mass, anchors - origins + centre, covariance, len(area)


def reduce_moments(keys, values):
    """Positive mixture reduction: mass, mean and centred covariance, in two passes."""
    keys, owner = np.unique(keys, return_inverse=True)
    count = len(keys)
    mass = np.bincount(owner, weights=values[:, 0], minlength=count)
    if np.any(mass <= 0):
        raise ValueError("positive conditional normal mass required")
    mean = np.column_stack(
        [
            np.bincount(owner, weights=values[:, 0] * values[:, i], minlength=count)
            / mass
            for i in (1, 2)
        ]
    )
    delta = values[:, 1:3] - mean[owner]
    covariance = np.empty((count, 2, 2))
    for i, j, at in ((0, 0, 3), (0, 1, 4), (1, 1, 5)):
        covariance[:, i, j] = (
            np.bincount(
                owner,
                weights=values[:, 0] * (values[:, at] + delta[:, i] * delta[:, j]),
                minlength=count,
            )
            / mass
        )
    covariance[:, 1, 0] = covariance[:, 0, 1]
    return keys, mass, mean, covariance


def sigma_points(mean, covariance):
    """Four positive points match degree two, not skewness or arbitrary BRDFs."""
    eigenvalues, vectors = np.linalg.eigh(covariance)
    tolerance = (
        256
        * np.finfo(float).eps
        * np.maximum(np.max(abs(covariance), axis=(1, 2)), np.finfo(float).tiny)
    )
    if np.any(eigenvalues[:, 0] < -tolerance):
        raise ValueError("conditional covariance is not positive semidefinite")
    axes = vectors * np.sqrt(2 * np.maximum(eigenvalues, 0))[:, None, :]
    points = np.stack(
        (
            mean + axes[:, :, 0],
            mean - axes[:, :, 0],
            mean + axes[:, :, 1],
            mean - axes[:, :, 1],
        ),
        axis=1,
    )
    return points, int((eigenvalues < 0).sum())


def program(arrays, geometry, view, box):
    _, basis, focal, width, height = view
    roi_width = int(box[1, 0] - box[0, 0])
    keys, density_values, polygons, pixel_rows, counts = [], [], [], [], []
    work = {
        "leaf_pixel_clips": 0,
        "geometric_fragments": 0,
        "geometric_fan_triangles": 0,
        "normal_contributions": 0,
        "pixel_normal_groups": 0,
        "quadrature_records": 0,
        "contribution_array_bytes": 0,
        "reduced_moment_array_bytes": 0,
        "clamped_roundoff_eigenvalues": 0,
    }
    for row, words, density, polygon in fragments(arrays, geometry, view, box, work):
        keep = density > 0
        words, density = words[keep], density[keep]
        keys.append((np.uint64(row) << np.uint64(32)) | words.astype(np.uint64))
        density_values.append(density)
        polygons.append(polygon)
        pixel_rows.append(row)
        counts.append(len(words))
    if not keys:
        return {
            "rows": np.empty(0, np.int32),
            "normal": np.empty((0, 3)),
            "weights": np.empty(0),
            "directions": np.empty((0, 3)),
            "work": work,
        }
    pixel_rows = np.asarray(pixel_rows)
    origins = box[0] + np.column_stack(
        (pixel_rows % roi_width + 0.5, pixel_rows // roi_width + 0.5)
    )
    area, mean, covariance, triangles = batch_moments(polygons, origins)
    work["geometric_fragments"] = len(polygons)
    work["geometric_fan_triangles"] = triangles
    geometric = np.column_stack(
        (area, mean, covariance[:, 0, 0], covariance[:, 0, 1], covariance[:, 1, 1])
    )
    values = np.repeat(geometric, counts, axis=0)
    values[:, 0] *= np.concatenate(density_values)
    keys = np.concatenate(keys)
    work["normal_contributions"] = len(keys)
    work["contribution_array_bytes"] = keys.nbytes + values.nbytes
    keys, mass, mean, covariance = reduce_moments(keys, values)
    points, clamped = sigma_points(mean, covariance)
    rows, words = (keys >> np.uint64(32)).astype(np.int32), keys.astype(np.uint32)
    origin = box[0] + np.column_stack((rows % roi_width + 0.5, rows // roi_width + 0.5))
    xy = (origin[:, None] + points).reshape(-1, 2)
    directions = (
        np.column_stack(
            (
                (xy[:, 0] - width / 2) / focal,
                -(xy[:, 1] - height / 2) / focal,
                np.ones(len(xy)),
            )
        )
        @ basis
    )
    work.update(
        pixel_normal_groups=len(keys),
        quadrature_records=len(xy),
        clamped_roundoff_eigenvalues=clamped,
        reduced_moment_array_bytes=keys.nbytes
        + mass.nbytes
        + mean.nbytes
        + covariance.nbytes,
    )
    return {
        "rows": np.repeat(rows, 4),
        "normal": np.repeat(unpack(words), 4, axis=0),
        "weights": np.repeat(mass / 4, 4),
        "directions": directions,
        "work": work,
    }
