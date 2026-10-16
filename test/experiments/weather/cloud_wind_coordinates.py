"""FP32 coordinate candidates; independent FP64 cloud field remains the reference."""

import itertools

import numpy as np
from cloud_field import altitude

F = np.float32
CELL_M = 1400.0
DETAIL = np.array([19, 7, 3], dtype=F)


def coordinates(points, velocity, seconds, kind, frequency=1.0, origin=None):
    origin = np.zeros(3) if origin is None else np.asarray(origin, dtype=float)
    if kind == "split":
        offset = (origin - velocity * seconds) / CELL_M * frequency
        base = np.floor(offset).astype(np.int64)
        fraction = (offset - base).astype(F)
        local = (points - origin).astype(F) / F(CELL_M) * F(frequency) + fraction
    else:
        phase = seconds % 3600 if kind == "modulo-hour" else seconds
        local = (points.astype(F) - (velocity * phase).astype(F)) / F(CELL_M)
        local *= F(frequency)
        base = np.zeros(3, dtype=np.int64)
    return (local + DETAIL if frequency != 1.0 else local), base


def hash_bits(cells):
    key = cells.astype(np.uint32).astype(np.uint64)
    bits = key[..., 0] * np.uint64(1597334677)
    bits ^= key[..., 1] * np.uint64(3812015801)
    bits ^= key[..., 2] * np.uint64(2798796415)
    bits &= np.uint64(0xFFFFFFFF)
    bits ^= bits >> 16
    bits = (bits * np.uint64(2246822519)) & np.uint64(0xFFFFFFFF)
    return bits ^ (bits >> 13)


def noise(local, base):
    cell = np.floor(local).astype(np.int64)
    fraction = local - cell.astype(F)
    blend = fraction * fraction * (F(3) - F(2) * fraction)
    result = np.zeros(local.shape[:-1], dtype=F)
    for corner in itertools.product((0, 1), repeat=3):
        corner = np.asarray(corner)
        bits = hash_bits(cell + corner + base)
        value = (bits >> 8).astype(F) / F(16777215)
        weight = np.prod(np.where(corner, blend, F(1) - blend), axis=-1, dtype=F)
        result += weight * value
    return result


def unit(value):
    value = np.clip(value, F(0), F(1))
    return value * value * (F(3) - F(2) * value)


def body(local, base, height, cover):
    cell = np.floor(local[:, (0, 2)]).astype(np.int64)
    fraction = local[:, (0, 2)] - cell.astype(F)
    result = np.zeros(len(local), dtype=F)
    for x, z in itertools.product(range(-1, 2), repeat=2):
        key = np.c_[
            cell[:, 0] + base[0] + x, np.full(len(cell), 812), cell[:, 1] + base[2] + z
        ]
        bits = hash_bits(key)
        features = [
            ((bits >> shift) & 255).astype(F) / F(255) for shift in (0, 8, 16, 24)
        ]
        cx = F(x) + F(0.5) + (features[0] - F(0.5)) * F(0.6)
        cz = F(z) + F(0.5) + (features[1] - F(0.5)) * F(0.6)
        radius = F(0.7) * np.sqrt(F(cover)) * (F(0.85) + F(0.3) * features[2])
        top = F(0.55) + F(0.4) * features[3]
        dx, dz = (fraction[:, 0] - cx) / radius, (fraction[:, 1] - cz) / radius
        vertical = height / top
        result = np.maximum(result, F(1) - (dx * dx + dz * dz) - vertical * vertical)
    return result


def density(points, layer, velocity, seconds, kind, profile, origin=None):
    """Isolate coordinate/algebra precision by sharing FP64 physical height across candidates."""
    height = (
        (altitude(points, layer.radius_m) - layer.base_m) / layer.thickness_m
    ).astype(F)
    active = (height > 0) & (height < 1)
    result = np.zeros(len(points), dtype=F)
    if layer.cover <= 0 or not np.any(active):
        return result
    selected = points[active]
    h = height[active]
    local, base = coordinates(selected, velocity, seconds, kind, origin=origin)
    detail_local, detail_base = coordinates(
        selected, velocity, seconds, kind, 3.1, origin
    )
    detail = noise(detail_local, detail_base)
    structure = F(0.8) * noise(local, base) + F(0.2) * detail
    stratus = (
        F(6) * h * (F(1) - h) * np.clip((structure - F(1 - layer.cover)) * F(2), 0, 1)
    )
    amount = stratus
    if profile == 1:
        eroded = body(local, base, h, layer.cover) - F(0.18) * (F(1) - detail)
        cumulus = F(1.5) * unit(h / F(0.08)) * unit(eroded)
        amount = (
            cumulus
            if layer.cover < 0.75
            else np.maximum(cumulus, unit(F((layer.cover - 0.75) / 0.25)) * stratus)
        )
    result[active] = F(layer.extinction_per_m) * amount
    return result
