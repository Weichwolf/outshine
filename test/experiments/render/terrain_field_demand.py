"""Visible-sample demand for the already qualified scalar R8 field sampler."""

import numpy as np


def pack_requests(keys, *, tile_bits=28):
    if tile_bits not in (28, 30):
        raise ValueError("bounded packed request domain required")
    bias = 1 << (tile_bits - 1)
    keys = np.asarray(keys)
    if (
        keys.ndim != 2
        or keys.shape[1] != 3
        or not np.issubdtype(keys.dtype, np.integer)
        or np.any(keys[:, :2] < (-bias + 1))
        or np.any(keys[:, :2] > (bias - 2))
        or np.any(keys[:, 2] < 0)
        or np.any(keys[:, 2] > 9)
    ):
        raise ValueError("supported lattice tile and mip addresses required")
    x = (keys[:, 0].astype(np.int64) + bias).astype(np.uint64)
    y = (keys[:, 1].astype(np.int64) + bias).astype(np.uint64)
    return (
        (x << np.uint64(tile_bits + 4))
        | (y << np.uint64(4))
        | keys[:, 2].astype(np.uint64)
    )


def unpack_requests(packed, *, tile_bits=28):
    if tile_bits not in (28, 30):
        raise ValueError("bounded packed request domain required")
    bias = 1 << (tile_bits - 1)
    values = np.asarray(packed, dtype=np.uint64)
    return np.column_stack(
        (
            (values >> np.uint64(tile_bits + 4)).astype(np.int64) - bias,
            ((values >> np.uint64(4)) & np.uint64((1 << tile_bits) - 1)).astype(
                np.int64
            )
            - bias,
            (values & np.uint64(15)).astype(np.int64),
        )
    ).astype(np.int32)


def _sample_plan(at_m, dx_m, dy_m, scale_m, lod_bias):
    at, dx, dy = [np.asarray(v, dtype=float) for v in (at_m, dx_m, dy_m)]
    if (
        at.ndim != 2
        or at.shape[1] != 2
        or dx.shape != at.shape
        or dy.shape != at.shape
        or not all(np.isfinite(v).all() for v in (at, dx, dy))
        or not np.isfinite(scale_m)
        or scale_m <= 0
        or not np.isfinite(lod_bias)
    ):
        raise ValueError(
            "finite visible positions, derivatives and wavelength required"
        )
    with np.errstate(over="ignore", divide="ignore"):
        dx, dy = dx / scale_m, dy / scale_m
    footprint = np.maximum(np.hypot(dx[:, 0], dx[:, 1]), np.hypot(dy[:, 0], dy[:, 1]))
    active = footprint < 2
    if not np.any(active):
        return np.empty((0, 2)), active, np.empty(0), np.empty(0), np.empty((0, 2))
    with np.errstate(over="ignore", divide="ignore"):
        at = at[active] / scale_m
    if not np.isfinite(at).all():
        raise ValueError("visible field position exceeds finite normalized coordinates")
    dx, dy = dx[active], dy[active]
    a = dx[:, 0] ** 2 + dy[:, 0] ** 2
    b = dx[:, 0] * dx[:, 1] + dy[:, 0] * dy[:, 1]
    c = dx[:, 1] ** 2 + dy[:, 1] ** 2
    separation = np.hypot(a - c, 2 * b)
    major2 = np.maximum((a + c + separation) * 0.5, 0)
    major = np.sqrt(major2)
    minor = np.sqrt(np.maximum((a + c - separation) * 0.5, 0))
    aspect = np.divide(major, minor, out=np.full(len(at), 8.0), where=minor > 0)
    count = np.ceil(np.clip(aspect, 1, 8))
    count[major <= 0.01] = 1
    effective_minor = np.maximum(minor, major / count)
    lod = np.clip(np.log2(np.maximum(effective_minor * 16, 1)) + lod_bias, 0, 9)
    axis = np.where(
        (a >= c)[:, None],
        np.column_stack((major2 - c, b)),
        np.column_stack((b, major2 - a)),
    )
    length = np.linalg.norm(axis, axis=1)
    axis = np.divide(
        axis,
        length[:, None],
        out=np.tile([1.0, 0.0], (len(at), 1)),
        where=length[:, None] > 0,
    )
    span = np.sqrt(np.maximum(major2 - effective_minor**2, 0))
    return at, active, lod, count, span[:, None] * axis


def requested_fields(at_m, dx_m, dy_m, scale_m, *, lod_bias=-0.5):
    at, active, lod, _, _ = _sample_plan(at_m, dx_m, dy_m, scale_m, lod_bias)
    tile = np.floor(at / 16)
    lower = np.floor(lod).astype(np.int32)
    first = np.column_stack((tile, lower))
    if len(first) and (np.any(tile < -134217727) or np.any(tile > 134217726)):
        raise ValueError("visible field tile exceeds native address bounds")
    first = first.astype(np.int32)
    second = first[lod > lower].copy()
    second[:, 2] += 1
    return np.unique(pack_requests(np.concatenate((first, second)))), active, lod


def requested_pages(at_m, dx_m, dy_m, scale_m, *, core_texels=64, lod_bias=-0.5):
    if (
        type(core_texels) is not int
        or core_texels < 1
        or core_texels > 512
        or core_texels & (core_texels - 1)
    ):
        raise ValueError("bounded power-of-two page core required")
    at, active, lod, count, offsets = _sample_plan(at_m, dx_m, dy_m, scale_m, lod_bias)
    lower = np.floor(lod).astype(np.int32)
    blocks = []
    for tap in range(int(count.max(initial=0))):
        selected = tap < count
        point = (
            at[selected]
            + offsets[selected] * ((tap + 0.5) / count[selected] - 0.5)[:, None]
        )
        for upper in (False, True):
            use = (
                lod[selected] > lower[selected]
                if upper
                else np.ones(selected.sum(), dtype=bool)
            )
            level = lower[selected][use] + int(upper)
            page_span = core_texels / 16 * np.exp2(level)
            tile = np.floor(point[use] / page_span[:, None])
            if np.any(np.abs(tile) > (1 << 29) - 2):
                raise ValueError("visible page exceeds packed address domain")
            keys = np.column_stack((tile, level)).astype(np.int32)
            blocks.append(np.unique(pack_requests(keys, tile_bits=30)))
    keys = np.unique(np.concatenate(blocks)) if blocks else np.empty(0, dtype=np.uint64)
    return keys, active, lod


def page_storage(packed, core_texels=64):
    levels = unpack_requests(packed, tile_bits=30)[:, 2]
    texels = len(packed) * (core_texels + 2) ** 2
    return {
        "products": len(packed),
        "package_bytes": texels + len(packed) * 16,
        "independent_page_texel_bytes": texels,
        "levels": np.bincount(levels, minlength=10).tolist(),
    }


def plane_samples(width, height, eye_height_m, fov_deg, pitch_deg, yaw_deg=0):
    if (
        type(width) is not int
        or type(height) is not int
        or min(width, height) <= 0
        or not np.isfinite([eye_height_m, fov_deg, pitch_deg, yaw_deg]).all()
        or eye_height_m <= 0
        or not 0 < fov_deg < 180
        or not -90 <= pitch_deg <= 90
    ):
        raise ValueError("finite perspective camera above a horizontal plane required")
    tangent = np.tan(np.deg2rad(fov_deg) * 0.5)
    sx, sy = np.meshgrid(
        ((np.arange(width) + 0.5) * 2 / width - 1) * tangent * width / height,
        (1 - (np.arange(height) + 0.5) * 2 / height) * tangent,
    )
    sine, cosine = np.sin(np.deg2rad(pitch_deg)), np.cos(np.deg2rad(pitch_deg))
    ray_y, ray_z = sine + sy.ravel() * cosine, cosine - sy.ravel() * sine
    visible = ray_y < 0
    t = -eye_height_m / ray_y[visible]
    x, z = sx.ravel()[visible], ray_z[visible]
    at = np.column_stack((t * x, t * z))
    step = 2 * tangent / height
    dx = np.column_stack((t * step, np.zeros(len(t))))
    dt = -eye_height_m * step * cosine / ray_y[visible] ** 2
    dy = np.column_stack((x * dt, z * dt + t * step * sine))
    near = np.hypot(np.linalg.norm(at, axis=1), eye_height_m) <= 240000
    at, dx, dy = at[near], dx[near], dy[near]
    flat = np.flatnonzero(visible)[near]
    angle = np.deg2rad(yaw_deg)
    rotation = np.array(
        [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
    )
    return at @ rotation.T, dx @ rotation.T, dy @ rotation.T, flat


def storage(packed):
    keys = unpack_requests(packed)
    sides = np.right_shift(512, keys[:, 2])
    texels = int(np.sum(sides.astype(np.int64) ** 2))
    return {
        "products": len(keys),
        "package_bytes": texels + len(keys) * 16,
        "independent_page_texel_bytes": texels,
        "levels": np.bincount(keys[:, 2], minlength=10).tolist(),
    }
