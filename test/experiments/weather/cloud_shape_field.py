"""Bounded low-cloud shape candidates; existing stratus remains the control."""

from dataclasses import dataclass

import numpy as np
from cloud_field import altitude, noise, wind

KINDS = ("stratus", "fbm-erosion", "cell-body", "cell-eroded", "cell-smooth")
CELL_M = 1400.0
DETAIL_OFFSET = np.array([19.0, 7.0, 3.0])


@dataclass
class Work:
    density_queries: int = 0
    in_shell_points: int = 0
    value_noise_points: int = 0
    lattice_hashes: int = 0
    body_cells: int = 0


def sampled_noise(points, work):
    if work is not None:
        work.value_noise_points += len(points)
        work.lattice_hashes += 8 * len(points)
    return noise(points)


def bits_for(x, z):
    x = x.astype(np.uint32).astype(np.uint64)
    z = z.astype(np.uint32).astype(np.uint64)
    bits = (x * np.uint64(1597334677)) ^ (z * np.uint64(2798796415))
    bits ^= np.uint64(812 * 3812015801)
    bits &= np.uint64(0xFFFFFFFF)
    bits ^= bits >> 16
    bits = (bits * np.uint64(2246822519)) & np.uint64(0xFFFFFFFF)
    bits ^= bits >> 13
    return bits


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def body_shape(advected, height, cover, work=None, neighborhood=1):
    cell = np.floor(advected[:, (0, 2)]).astype(np.int64)
    body = np.zeros(len(advected))
    for x in range(-neighborhood, neighborhood + 1):
        for z in range(-neighborhood, neighborhood + 1):
            cx, cz = cell[:, 0] + x, cell[:, 1] + z
            bits = bits_for(cx, cz)
            channels = [
                ((bits >> shift) & 255).astype(float) / 255 for shift in (0, 8, 16, 24)
            ]
            center_x = cx + 0.5 + (channels[0] - 0.5) * 0.6
            center_z = cz + 0.5 + (channels[1] - 0.5) * 0.6
            radius = 0.7 * np.sqrt(cover) * (0.85 + 0.3 * channels[2])
            top = 0.55 + 0.4 * channels[3]
            q = (
                ((advected[:, 0] - center_x) / radius) ** 2
                + ((advected[:, 2] - center_z) / radius) ** 2
                + (height / top) ** 2
            )
            body = np.maximum(body, 1 - q)
            if work is not None:
                work.body_cells += len(advected)
                work.lattice_hashes += len(advected)
    return body


def extinction(points, layer, seconds, kind="stratus", work=None, drift_m=None):
    shape = points.shape[:-1]
    points = points.reshape(-1, 3)
    output = np.zeros(len(points))
    if work is not None:
        work.density_queries += len(points)
    if layer.cover <= 0:
        return output.reshape(shape)
    height = (altitude(points, layer.radius_m) - layer.base_m) / layer.thickness_m
    active = (height > 0) & (height < 1)
    if work is not None:
        work.in_shell_points += int(np.sum(active))
    if not np.any(active):
        return output.reshape(shape)
    h = height[active]
    drift = wind(layer) * seconds if drift_m is None else drift_m
    advected = (points[active] - drift) / CELL_M
    if kind in ("cell-body", "cell-eroded", "cell-smooth"):
        body = body_shape(advected, h, layer.cover, work)
        amount = np.zeros(len(body))
        inside = body > 0
        if kind == "cell-smooth" and layer.cover >= 0.75:
            detail = sampled_noise(advected * 3.1 + DETAIL_OFFSET, work)
            broad = sampled_noise(advected, work)
            amount = smooth(0, 1, body - 0.18 * (1 - detail))
            cumulus = 1.5 * smooth(0, 0.08, h) * amount
            stratus = (
                6
                * h
                * (1 - h)
                * np.clip((0.8 * broad + 0.2 * detail - (1 - layer.cover)) * 2, 0, 1)
            )
            output[active] = layer.extinction_per_m * np.maximum(
                cumulus, smooth(0.75, 1, layer.cover) * stratus
            )
            return output.reshape(shape)
        if np.any(inside):
            frequency, erosion = (7.3, 0.45) if kind == "cell-eroded" else (3.1, 0.18)
            detail = sampled_noise(advected[inside] * frequency + DETAIL_OFFSET, work)
            core = body[inside] - erosion * (1 - detail)
            amount[inside] = (
                smooth(0, 1, core) if kind == "cell-smooth" else np.clip(3 * core, 0, 1)
            )
        output[active] = layer.extinction_per_m * 1.5 * smooth(0, 0.08, h) * amount
    elif kind in ("stratus", "fbm-erosion"):
        broad = sampled_noise(advected, work)
        detail = sampled_noise(advected * 3.1 + DETAIL_OFFSET, work)
        amount = np.clip((0.8 * broad + 0.2 * detail - (1 - layer.cover)) * 2, 0, 1)
        profile = 6 * h * (1 - h)
        if kind == "fbm-erosion":
            fine = sampled_noise(advected * 7.3 + np.array([3, 29, 11]), work)
            amount = np.maximum(0, amount - 0.15 * (1 - fine))
            top = 0.35 + 0.65 * smooth(0.25, 0.75, broad)
            profile = 1.5 * smooth(0, 0.08, h) * (1 - smooth(0.6 * top, top, h))
        output[active] = layer.extinction_per_m * profile * amount
    else:
        raise ValueError(kind)
    return output.reshape(shape)
