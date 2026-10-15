"""Declared procedural cloud candidates; metres in the local east/up/south frame."""

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class Layer:
    base_m: float = 2572.0
    thickness_m: float = 600.0
    cover: float = 1.0
    extinction_per_m: float = 0.012
    radius_m: float = 6360000.0
    wind_from_deg: float = 240.0
    wind_ms: float = 8.0


def wind(layer):
    bearing = np.deg2rad(layer.wind_from_deg)
    return layer.wind_ms * np.array([-np.sin(bearing), 0.0, np.cos(bearing)])


def noise(position):
    cell = np.floor(position).astype(np.int64)
    fraction = position - cell
    blend = fraction * fraction * (3.0 - 2.0 * fraction)
    result = np.zeros(position.shape[:-1])
    for x in range(2):
        for y in range(2):
            for z in range(2):
                corner = np.array([x, y, z])
                key = (cell + corner).astype(np.uint32).astype(np.uint64)
                bits = key[..., 0] * np.uint64(1597334677)
                bits ^= key[..., 1] * np.uint64(3812015801)
                bits ^= key[..., 2] * np.uint64(2798796415)
                bits &= np.uint64(0xFFFFFFFF)
                bits ^= bits >> 16
                bits = (bits * np.uint64(2246822519)) & np.uint64(0xFFFFFFFF)
                bits ^= bits >> 13
                sample = (bits >> 8).astype(np.float64) / 16777215.0
                weight = np.prod(np.where(corner, blend, 1.0 - blend), axis=-1)
                result += weight * sample
    return result


def altitude(position, radius):
    relative_square = (
        np.sum(position * position, axis=-1) + 2.0 * radius * position[..., 1]
    )
    return relative_square / (np.sqrt(relative_square + radius * radius) + radius)


def density(position, layer, seconds=0.0):
    height = (altitude(position, layer.radius_m) - layer.base_m) / layer.thickness_m
    profile = np.where(
        (height > 0.0) & (height < 1.0), 6.0 * height * (1.0 - height), 0.0
    )
    advected = (position - wind(layer) * seconds) / 1400.0
    broad = noise(advected)
    detail = noise(advected * 3.1 + np.array([19.0, 7.0, 3.0]))
    structure = 0.8 * broad + 0.2 * detail
    amount = np.clip((structure - (1.0 - layer.cover)) * 2.0, 0.0, 1.0)
    return layer.extinction_per_m * profile * amount


def sphere_roots(origin, direction, height_m, radius_m):
    b = np.sum(origin * direction, axis=-1) + radius_m * direction[..., 1]
    c = (
        np.sum(origin * origin, axis=-1)
        + 2.0 * radius_m * origin[..., 1]
        - height_m * (2.0 * radius_m + height_m)
    )
    discriminant = b * b - c
    root = np.sqrt(np.maximum(discriminant, 0.0))
    return -b - root, -b + root, discriminant >= 0.0


def segments(origin, direction, layer):
    """Up to two forward shell intervals, clipped by the opaque spherical planet."""
    outer_near, outer_far, outer_hit = sphere_roots(
        origin, direction, layer.base_m + layer.thickness_m, layer.radius_m
    )
    inner_near, inner_far, inner_hit = sphere_roots(
        origin, direction, layer.base_m, layer.radius_m
    )
    ground_near, _, ground_hit = sphere_roots(origin, direction, 0.0, layer.radius_m)
    start = np.maximum(outer_near, 0.0)
    inward = (
        np.sum(origin * direction, axis=-1) + layer.radius_m * direction[..., 1] < 0.0
    )
    ground_blocks = ground_hit & inward & (ground_near >= 0.0)
    stop = np.minimum(outer_far, np.where(ground_blocks, ground_near, np.inf))
    inner_intersects = inner_hit & (inner_far > start) & (inner_near < stop)
    split = np.minimum(np.maximum(inner_near, start), stop)
    first_stop = np.where(inner_intersects, split, stop)
    second_start = np.maximum(inner_far, start)
    valid = outer_hit & (stop > start)
    first = np.stack([start, np.maximum(start, first_stop)], axis=-1)
    second = np.stack([second_start, np.maximum(second_start, stop)], axis=-1)
    first = np.where(valid[..., None], first, 0.0)
    second = np.where((valid & inner_intersects)[..., None], second, 0.0)
    promote = (first[..., 1] == first[..., 0]) & (second[..., 1] > second[..., 0])
    first = np.where(promote[..., None], second, first)
    second = np.where(promote[..., None], 0.0, second)
    first = np.where((first[..., 1] > first[..., 0])[..., None], first, 0.0)
    second = np.where((second[..., 1] > second[..., 0])[..., None], second, 0.0)
    return np.stack([first, second], axis=-2)


def optical_depth(origin, direction, layer, steps, seconds=0.0, maximum_m=np.inf):
    intervals = segments(origin, direction, layer)
    optical = np.zeros(intervals.shape[:-2])
    for interval in range(2):
        start = intervals[..., interval, 0]
        stop = np.maximum(start, np.minimum(intervals[..., interval, 1], maximum_m))
        if not np.any(stop > start):
            continue
        step_m = (stop - start) / steps
        for sample in range(steps):
            distance = start + (sample + 0.5) * step_m
            position = origin + distance[..., None] * direction
            optical += density(position, layer, seconds) * step_m
    return optical


def transmittance(origin, direction, layer, steps, seconds=0.0):
    return np.exp(-optical_depth(origin, direction, layer, steps, seconds))
