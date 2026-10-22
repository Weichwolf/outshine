"""Area-weighted signed scene corrections within projected coarse support."""

import itertools
import time

import numpy as np
from pixel_polygon_quadrature import convex_hull, pixel_quadrature


def support_points(product, height, profile):
    low = product["reference_origin_m"][:2]
    high = low + product["shape"] * product["header"]["pitch_m"]
    corners = np.array(list(itertools.product(*zip(low, high, strict=True))))
    if profile["kind"] == "point":
        light = np.asarray(profile["position"], float)
        if light[2] <= height:
            raise ValueError("finite flat-base support requires light above model")
        shadows = corners + (corners - light[:2]) * height / (light[2] - height)
    else:
        light = np.asarray(profile["direction"], float)
        if light[2] <= 0:
            raise ValueError("finite flat-base support requires positive light z")
        shadows = corners - height * light[:2] / light[2]
    return np.vstack(
        (
            np.c_[corners, np.zeros(4)],
            np.c_[corners, np.full(4, height)],
            np.c_[shadows, np.zeros(4)],
        )
    )


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


def filter_correction(polygon, box, view, scene, background, *, order):
    """Requery the complete scene at every node before filtering its difference.

    The caller owns the independently filtered baseline. A coarse support polygon
    bounds only this model's signed difference, never real-world visibility.
    """
    started = time.perf_counter()
    xy, owners, weights, work = pixel_quadrature(polygon, box, order=order)
    geometry_seconds = time.perf_counter() - started
    width, height = box[1] - box[0]
    delta, coverage = np.zeros((width * height, 3)), np.zeros(width * height)
    transport = {}
    started = time.perf_counter()
    if len(xy):
        origins, directions = screen_rays(view, xy)
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
