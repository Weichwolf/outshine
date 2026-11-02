"""Independent double-precision page filtering and native query corpus."""

import numpy as np
from native_litter_page_sampling import bilinear
from terrain_field_demand import _sample_plan


def filtered_queries(keys, core, rng):
    mip = min(key[2] for key in keys)
    origin = np.min(np.asarray([k[:2] for k in keys if k[2] == mip]), axis=0)
    point = (origin + rng.uniform(0.25, 1.75, (512, 2))) * core * 2**mip / 16
    minor = np.exp2(rng.uniform(mip + 0.1, mip + 0.4, len(point)) + 0.5) / 16
    ratio = rng.choice([1.05, 2.25, 4.25, 7.5], len(point))
    major = np.minimum(minor * ratio, 1.95)
    angle = rng.uniform(-np.pi, np.pi, len(point))
    e = np.column_stack((np.cos(angle), np.sin(angle)))
    f = np.column_stack((-np.sin(angle), np.cos(angle)))
    turn = rng.uniform(-np.pi, np.pi, (len(point), 1))
    dx = major[:, None] * e * np.cos(turn) + minor[:, None] * f * np.sin(turn)
    dy = -major[:, None] * e * np.sin(turn) + minor[:, None] * f * np.cos(turn)
    values = np.zeros((len(point) + 4, 12), dtype="<f4")
    values[:-4, :2], values[:-4, 2:4], values[:-4, 4:6] = point, dx, dy
    values[-4:, :2] = [[0, 0], [1e8, -1e8], [-1e8, 1e8], [1e8, -1e8]]
    values[-3:, 2] = [2, 2.01, 1.999]
    values.view("<u4")[:, 11] = 1
    return values


def filter_reference(queries, pixels, present, core):
    at, active, lod, count, offsets = _sample_plan(
        queries[:, :2], queries[:, 2:4], queries[:, 4:6], 1, -0.5
    )
    ready = np.ones(len(at), dtype=bool)
    total = np.zeros(len(at))
    lower = np.floor(lod).astype(int)
    first_missing = np.zeros((len(at), 3), dtype=np.int32)
    for tap in range(int(count.max(initial=0))):
        use = np.flatnonzero(tap < count)
        point = at[use] + offsets[use] * ((tap + 0.5) / count[use] - 0.5)[:, None]
        for upper in (0, 1):
            needed = (
                np.ones(len(use), dtype=bool) if not upper else lod[use] > lower[use]
            )
            where = use[needed]
            level = lower[where] + upper
            texel = point[needed] * np.exp2(4 - level[:, None])
            tile = np.floor(texel / core).astype(np.int32)
            key = np.column_stack((tile, level))
            exists = np.asarray([tuple(k) in present for k in key])
            first = ready[where] & ~exists
            first_missing[where[first]] = key[first]
            ready[where] &= exists
            amount = lod[where] - lower[where]
            if not upper:
                amount = 1 - amount
            for k in present:
                selected = np.all(key == k, axis=1)
                local = texel[selected] - np.array(k[:2]) * core + 1
                total[where[selected]] += (
                    bilinear(pixels[k], local / (core + 2))
                    * amount[selected]
                    / count[where[selected]]
                )
    normalized_dx = queries[active, 2:4].astype(float)
    normalized_dy = queries[active, 4:6].astype(float)
    footprint = np.maximum(
        np.linalg.norm(normalized_dx, axis=1), np.linalg.norm(normalized_dy, axis=1)
    )
    t = np.clip(footprint - 1, 0, 1)
    fade = 1 - t * t * (3 - 2 * t)
    values = np.zeros((len(queries), 4))
    values[active] = np.column_stack(((total - 0.5) * fade, lod, count, fade))
    values[np.flatnonzero(active)[~ready]] = 0
    found = np.ones(len(queries), dtype=bool)
    found[active] = ready
    missing = np.zeros((len(queries), 3), dtype=np.int32)
    missing[np.flatnonzero(active)[~ready]] = first_missing[~ready]
    return values, found, missing
