"""Area-weighted signed scene corrections within projected coarse support."""

import time

import numpy as np
from pixel_polygon_quadrature import convex_hull, pixel_quadrature


def support_polygon(points, view):
    eye, basis, focal, width, height = view
    p = (points - eye) @ basis.T
    if np.any(p[:, 2] <= 0.1):
        raise ValueError("support must lie ahead of the near plane")
    return convex_hull(
        [width / 2, height / 2] + focal * p[:, :2] / p[:, 2, None] * [1, -1]
    )


def screen_rays(view, xy):
    eye, basis, focal, width, height = view
    d = (
        np.column_stack(
            (
                (xy[:, 0] - width / 2) / focal,
                (height / 2 - xy[:, 1]) / focal,
                np.ones(len(xy)),
            )
        )
        @ basis
    )
    d /= np.linalg.norm(d, axis=1)[:, None]
    return np.broadcast_to(eye, d.shape).copy(), d


def filter_correction(polygon, box, view, scene, background, *, order, interior=None):
    """Requery the complete scene at every node before filtering its difference.

    The caller owns the independently filtered baseline. A coarse support polygon
    bounds only this model's signed difference, never real-world visibility.
    """
    started = time.perf_counter()
    xy, owners, weights, work = pixel_quadrature(
        polygon, box, order=order, skip_full=interior is not None
    )
    full = np.asarray(work.pop("full_pixel_ids"), np.int64)
    width, height = box[1] - box[0]
    origins, directions = screen_rays(view, xy)
    work["retained_full_pixels"] = len(full)
    work["retained_uniform_rays"] = 0
    if interior is not None and len(full):
        old_o, old_d, old_ids = interior
        take = np.isin(old_ids, full)
        ids = old_ids[take]
        counts = np.bincount(ids, minlength=width * height)
        if np.any(counts[full] == 0):
            raise ValueError("uniform fallback must cover every full support pixel")
        origins = np.vstack((origins, old_o[take]))
        directions = np.vstack((directions, old_d[take]))
        owners = np.r_[owners, ids]
        weights = np.r_[weights, 1 / counts[ids]]
        work["retained_uniform_rays"] = len(ids)
    work["scene_rays"] = len(origins)
    work["ray_arrays_bytes"] = (
        origins.nbytes + directions.nbytes + owners.nbytes + weights.nbytes
    )
    geometry_seconds = time.perf_counter() - started
    delta, coverage = np.zeros((width * height, 3)), np.zeros(width * height)
    transport = {}
    started = time.perf_counter()
    if len(origins):
        rgb, alpha, transport = scene(origins, directions)
        difference = rgb - background(origins, directions)
        for channel in range(3):
            delta[:, channel] = np.bincount(
                owners,
                weights=weights * difference[:, channel],
                minlength=width * height,
            )
        coverage[:] = np.bincount(
            owners, weights=weights * alpha, minlength=width * height
        )
    work.update(
        geometry_seconds=geometry_seconds,
        transport=transport,
        transport_seconds=time.perf_counter() - started,
    )
    return delta.reshape(height, width, 3), coverage.reshape(height, width), work
