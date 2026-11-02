"""Additive fixed normal-sector moments; approximate GGX, no temporal sampling."""

import numpy as np
from relief_shading_reference import normals, roughened, specular
from terrain_material_grid import frozen


def _components(raw):
    n = normals(raw[:, 4:])
    labels = (n[:, 0] < 0).astype(int) + 2 * (n[:, 1] < 0)
    fields = np.column_stack(
        (
            np.ones(len(raw)),
            n,
            np.clip(raw[:, 3], 0.04, 1) ** 4,
            (np.clip(raw[:, :3], 0, 1)[:, :, None] * n[:, None, :]).reshape(-1, 9),
        )
    )
    return labels, fields


def accumulate(raw, weights):
    """Four persistent labels; addition/filtering never needs to match fitted lobes."""
    raw, weights = np.asarray(raw, dtype=float), np.asarray(weights, dtype=float)
    if (
        raw.ndim != 2
        or raw.shape[1] != 6
        or weights.shape != (len(raw),)
        or not np.isfinite(raw).all()
        or not np.isfinite(weights).all()
        or np.any(weights < 0)
        or not np.isclose(weights.sum(), 1, rtol=1e-12, atol=1e-12)
    ):
        raise ValueError("finite joint material samples and normalized areas required")
    labels, fields = _components(raw)
    result = np.zeros((4, 14))
    np.add.at(result, labels, fields * weights[:, None])
    return result


def response(moments, lights, view):
    """Colour/normal covariance for diffuse; bounded Toksvig GGX per normal sector.

    Diffuse is exact when each sector lies wholly above or below the light horizon.
    Mixed horizon sectors and all specular convolution remain approximations.
    """
    moments = np.asarray(moments, dtype=float)
    if (
        moments.shape != (4, 14)
        or not np.isfinite(moments).all()
        or np.any(moments[:, 0] < 0)
        or not np.isclose(moments[:, 0].sum(), 1, rtol=1e-6, atol=1e-8)
    ):
        raise ValueError("four finite normalized sector moments required")
    selected = moments[moments[:, 0] > 0]
    area = selected[:, 0]
    mean = selected[:, 1:4] / area[:, None]
    length = np.linalg.norm(mean, axis=-1)
    normal = mean / length[:, None]
    roughness = roughened((selected[:, 4] / area) ** 0.25, np.minimum(length, 1))
    colour_normal = selected[:, 5:].reshape(-1, 3, 3)
    half = lights + view
    half /= np.linalg.norm(half, axis=-1, keepdims=True)
    fresnel = 0.04 + 0.96 * (1 - np.clip(half @ view, 0, 1)) ** 5
    return np.array(
        [
            (1 - f) * np.maximum(colour_normal @ light, 0).sum(axis=0) / np.pi
            + area @ specular(normal, roughness, light, view)
            for light, f in zip(lights, fresnel, strict=True)
        ]
    )


def pyramid(source):
    """Mips of additive moments from the same stored fine basis, without refitting."""
    raw = source.levels[0].fields[:, :, 0].reshape(-1, 6).astype(float)
    labels, fields = _components(raw)
    width = len(source.levels[0].fields)
    values = np.zeros((len(raw), 4, 14))
    values[np.arange(len(raw)), labels] = fields
    values = values.reshape(width, width, 4, 14)
    levels = [frozen(values.astype(np.float32))]
    while width > 1:
        width //= 2
        values = values.reshape(width, 2, width, 2, 4, 14).mean(axis=(1, 3))
        levels.append(frozen(values.astype(np.float32)))
    return tuple(levels)


def sample(levels, queries):
    """Bounded centre-trilinear moment filter; anisotropic reconstruction is absent."""
    queries = np.asarray(queries, dtype=float)
    if (
        queries.ndim != 2
        or queries.shape[1] != 8
        or not 1 <= len(queries) <= 262144
        or not np.isfinite(queries).all()
        or np.any(queries[:, 6] < 0)
        or np.any(queries[:, 6] > len(levels) - 1)
        or np.any(
            np.abs(queries[:, 2] * queries[:, 5] - queries[:, 3] * queries[:, 4])
            <= 1e-20
        )
    ):
        raise ValueError("bounded nonsingular resident material queries required")
    reach = (np.abs(queries[:, 2:4]) + np.abs(queries[:, 4:6])) / 2
    if np.any(queries[:, :2] - reach < 0) or np.any(queries[:, :2] + reach > 1):
        raise ValueError("footprint exceeds resident tile")
    first = np.floor(queries[:, 6]).astype(int)
    fraction = queries[:, 6] - first
    result = np.zeros((len(queries), 4, 14))
    for delta in (0, 1):
        indices = np.minimum(first + delta, len(levels) - 1)
        amount = 1 - fraction if delta == 0 else fraction
        for index in np.unique(indices[amount > 0]):
            selected = (indices == index) & (amount > 0)
            level = levels[index]
            width = len(level)
            at = queries[selected, :2] * width - 0.5
            if width > 1 and (np.any(at < 0) or np.any(at > width - 1)):
                raise ValueError("bilinear stencil needs a resident neighbour halo")
            low = np.floor(at).astype(int)
            t = at - low
            value = np.zeros((len(at), 4, 14))
            for x, y in ((0, 0), (1, 0), (0, 1), (1, 1)):
                xy = np.clip(low + [x, y], 0, width - 1)
                weight = (t[:, 0] if x else 1 - t[:, 0]) * (
                    t[:, 1] if y else 1 - t[:, 1]
                )
                value += level[xy[:, 1], xy[:, 0]] * weight[:, None, None]
            result[selected] += value * amount[selected, None, None]
    return result
