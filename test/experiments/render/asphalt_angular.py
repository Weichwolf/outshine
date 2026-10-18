"""Own compact angular aggregate study; periodic authored sources, not measured asphalt."""

import itertools
from dataclasses import dataclass

import numpy as np
import shapely
from asphalt_box_reference import interval
from asphalt_field import SIGMA, grain
from PIL import Image, ImageDraw


@dataclass(frozen=True)
class Aggregate:
    centre: np.ndarray
    vertices: np.ndarray
    weight: float


def source_site(x, y, period, index):
    key = np.array([x % period + index * 127, y % period - index * 8191])
    site, weight = grain(key[None])
    centre = np.array([x, y]) + site[0] - key
    seed = int((int(key[0]) * 0x9E3779B1 ^ int(key[1]) * 0x85EBCA77) & 0xFFFFFFFF)
    return centre, float(weight[0]), seed


def aggregates(period, density, padding=1):
    if (
        type(period) is not int
        or type(density) is not int
        or period not in (8, 16, 32)
        or density not in (1, 2, 4)
        or padding not in (1, 3)
    ):
        raise ValueError("bounded explicit period and density required")
    for x, y in itertools.product(range(-padding, period + padding), repeat=2):
        for index in range(density):
            centre, weight, seed = source_site(x, y, period, index)
            rng = np.random.default_rng(seed)
            angles = np.arange(8) * (np.pi / 4) + rng.uniform(0, 2 * np.pi)
            radii = rng.uniform(0.14, 0.22, 8)
            points = centre + radii[:, None] * np.c_[np.cos(angles), np.sin(angles)]
            polygon = shapely.convex_hull(shapely.multipoints(points))
            vertices = np.asarray(polygon.exterior.coords)[:-1].copy()
            a, b = vertices[1] - vertices[0], vertices[2] - vertices[1]
            signed = a[0] * b[1] - a[1] * b[0]
            if signed < 0:
                vertices = vertices[::-1].copy()
            centre.setflags(write=False)
            vertices.setflags(write=False)
            yield Aggregate(centre, vertices, weight / np.sqrt(density))


def pixel_bounds(vertices, scale, resolution):
    low = np.maximum(np.floor(vertices.min(axis=0) * scale).astype(int), 0)
    high = np.minimum(np.ceil(vertices.max(axis=0) * scale).astype(int), resolution)
    return int(low[0]), int(low[1]), int(high[0]), int(high[1])


def coverage(vertices, box, scale, method):
    x0, y0, x1, y1 = box
    xx, yy = np.meshgrid(np.arange(x0, x1), np.arange(y0, y1))
    if method == "area":
        pixels = shapely.box(xx / scale, yy / scale, (xx + 1) / scale, (yy + 1) / scale)
        return (
            shapely.area(shapely.intersection(shapely.Polygon(vertices), pixels))
            * scale**2
        )
    if method == "point":
        points = np.stack(((xx + 0.5) / scale, (yy + 0.5) / scale), axis=-1)
        edges = np.roll(vertices, -1, axis=0) - vertices
        delta = points[..., None, :] - vertices
        return (edges[:, 0] * delta[..., 1] - edges[:, 1] * delta[..., 0] >= 0).all(
            axis=-1
        )
    if method not in ("ss8", "ss16", "ss32"):
        raise ValueError("declared coverage method required")
    samples = int(method.removeprefix("ss"))
    mask = Image.new("1", ((x1 - x0) * samples, (y1 - y0) * samples))
    polygon = (vertices * scale - [x0, y0]) * samples - 0.5
    ImageDraw.Draw(mask).polygon([tuple(p) for p in polygon], fill=1)
    return (
        np.asarray(mask).reshape(y1 - y0, samples, x1 - x0, samples).mean(axis=(1, 3))
    )


def angular_tile(period=16, density=1, resolution=512, method="area", binder="uniform"):
    if (
        type(resolution) is not int
        or not 1 <= resolution <= 1024
        or resolution & (resolution - 1)
    ):
        raise ValueError("declared bounded tile resolution required")
    if method not in ("area", "point", "ss8", "ss16", "ss32", "ss32-dc"):
        raise ValueError("declared coverage method required")
    if binder not in ("uniform", "halo"):
        raise ValueError("declared binder control required")
    field = np.zeros((resolution, resolution))
    scale = resolution / period
    dc = 0.0
    work = {
        "source_grains": 0,
        "clipped_polygons": 0,
        "pixel_pairs": 0,
        "peak_local_pixels": 0,
    }
    for item in aggregates(period, density):
        work["source_grains"] += 1
        if (item.centre >= 0).all() and (item.centre < period).all():
            dc += 0.75 * item.weight * shapely.area(shapely.Polygon(item.vertices))
        kernels = ((1, 0.75),) if binder == "uniform" else ((1, 1), (2, -0.25))
        for factor, coefficient in kernels:
            vertices = item.centre + factor * (item.vertices - item.centre)
            box = pixel_bounds(vertices, scale, resolution)
            x0, y0, x1, y1 = box
            if x1 <= x0 or y1 <= y0:
                continue
            count = (x1 - x0) * (y1 - y0)
            if count > 16384:
                raise ValueError("declared local pixel scratch exceeded")
            field[y0:y1, x0:x1] += (
                item.weight
                * coefficient
                * coverage(vertices, box, scale, method.removesuffix("-dc"))
            )
            work["clipped_polygons"] += 1
            work["pixel_pairs"] += count
            work["peak_local_pixels"] = max(work["peak_local_pixels"], count)
    if binder == "uniform":
        field -= dc / period**2
    work["analytic_dc"] = dc / period**2 if binder == "uniform" else 0.0
    work["numerical_dc_correction"] = (
        float(field.mean()) if method.endswith("-dc") else 0.0
    )
    field -= work["numerical_dc_correction"]
    return field, work


def mip_fields(field):
    if (
        field.ndim != 2
        or field.shape[0] != field.shape[1]
        or not np.isfinite(field).all()
    ):
        raise ValueError("finite square scalar field required")
    size = field.shape[0]
    if not 1 <= size <= 1024 or size & (size - 1):
        raise ValueError("bounded power-of-two field required")
    levels = [field]
    while levels[-1].shape[0] > 1:
        size = levels[-1].shape[0] // 2
        levels.append(levels[-1].reshape(size, 2, size, 2).mean(axis=(1, 3)))
    return tuple(levels)


def gaussian_tile(period=16, resolution=512):
    if period not in (8, 16, 32) or resolution not in (256, 512, 1024):
        raise ValueError("declared Gaussian control dimensions required")
    field = np.zeros((resolution, resolution))
    scale = resolution / period
    for x, y in itertools.product(range(-3, period + 3), repeat=2):
        centre, weight, _ = source_site(x, y, period, 0)
        vertices = np.array([centre - 16 * SIGMA, centre + 16 * SIGMA])
        x0, y0, x1, y1 = pixel_bounds(vertices, scale, resolution)
        if x1 <= x0 or y1 <= y0:
            continue
        px = (np.arange(x0, x1) + 0.5) / scale - centre[0]
        py = (np.arange(y0, y1) + 0.5) / scale - centre[1]
        for sigma, coefficient in ((SIGMA, 1), (2 * SIGMA, -0.25)):
            field[y0:y1, x0:x1] += (
                coefficient
                * weight
                * np.outer(
                    interval(py, 1 / scale, sigma), interval(px, 1 / scale, sigma)
                )
            )
    return field
