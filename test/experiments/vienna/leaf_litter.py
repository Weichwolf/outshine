"""Bake a repeating layer of decaying broadleaves over humus and moss."""

import numpy as np


def bake_litter(rng, fine, grain, humus, moss_field, deposition, span_m):
    size = fine.shape[0]
    pitch = span_m / size
    tint = 0.63 + 0.10 * humus + 0.055 * grain
    moss = np.clip(0.47 + 0.27 * moss_field + 0.10 * grain, 0.0, 1.0)
    tint += 0.29 * moss
    roughness = 0.94 - 0.11 * moss + 0.018 * fine
    height = 0.00038 * fine + 0.0012 * humus + 0.0014 * moss
    # Sample overlapping deposits at 0.005 m/texel on the current 512 texel atlas.
    seeds = rng.uniform(0, size, (round(span_m**2 * 610), 2))
    chance = np.clip(0.64 + 0.25 * deposition, 0.20, 0.94)
    x, y = seeds.astype(int).T
    seeds = seeds[rng.random(len(seeds)) < chance[y, x]]
    for x, y in seeds:
        length = rng.uniform(0.035, 0.117) / pitch
        width = length * rng.uniform(0.40, 0.64)
        angle = rng.uniform(0, 2 * np.pi)
        ca, sa = np.cos(angle), np.sin(angle)
        radius = (length + width) * 0.5 + 1
        columns = np.arange(np.floor(x - radius), np.ceil(x + radius), dtype=int)
        rows = np.arange(np.floor(y - radius), np.ceil(y + radius), dtype=int)
        east, north = columns[None] + 0.5 - x, rows[:, None] + 0.5 - y
        u = (ca * east + sa * north) / length + 0.5
        v = (-sa * east + ca * north) / (0.5 * width)
        profile = np.sin(np.pi * np.clip(u, 0, 1)) ** 0.85
        lobes = 0.90 + 0.10 * np.sin(8 * np.pi * u + rng.uniform(-1, 1))
        edge = np.minimum(
            np.minimum(u, 1 - u) * length, (profile * lobes - abs(v)) * width * 0.5
        )
        coverage = np.clip(edge + 0.4, 0, 1)
        region = np.ix_(rows % size, columns % size)
        rib = np.exp(-18 * np.abs(v))
        decay = rng.uniform(0.83, 1.64)
        pigment = decay + 0.075 * grain[region] + 0.06 * rib
        curled = rng.uniform(0.0020, 0.0053) * (0.38 + 0.62 * profile) + 0.00065 * rib
        tint[region] += coverage * (pigment - tint[region])
        roughness[region] += coverage * (rng.uniform(0.68, 0.84) - roughness[region])
        height[region] += coverage * (
            np.maximum(curled, height[region]) - height[region]
        )
        moss[region] *= 1 - coverage
    return tint, roughness, height, moss
