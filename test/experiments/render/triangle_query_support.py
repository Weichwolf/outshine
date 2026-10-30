"""Conservative projected whole-source box; uncertain projection keeps all work."""

import numpy as np


def lower(value):
    return np.nextafter(value, -np.inf)


def upper(value):
    return np.nextafter(value, np.inf)


def screen_bounds(source_bounds, world, raster):
    """Enclose every source point; require positive w throughout its whole box.

    Separate outward-rounded products/sums/divisions bound the binary64 inputs.
    This is an acceleration bound, not a native raster coverage certificate.
    """
    bounds = np.asarray(source_bounds, dtype=float)
    bits = np.array([[i & 1, (i >> 1) & 1, (i >> 2) & 1] for i in range(8)])
    corners = np.c_[bounds[bits, np.arange(3)], np.ones(8)]
    with np.errstate(over="ignore", invalid="ignore", divide="ignore"):
        products = corners[:, None, :] * world[None, :, :]
        lo, hi = lower(products[:, :, 0]), upper(products[:, :, 0])
        for axis in range(1, 4):
            lo = lower(lo + lower(products[:, :, axis]))
            hi = upper(hi + upper(products[:, :, axis]))
        if (
            not np.isfinite(lo).all()
            or not np.isfinite(hi).all()
            or (lo[:, 3] <= 0).any()
        ):
            return None
        quotients = np.stack(
            [n[:, :2] / d[:, 3, None] for n in (lo, hi) for d in (lo, hi)]
        )
        ndc_lo = lower(quotients).min(axis=(0, 1))
        ndc_hi = upper(quotients).max(axis=(0, 1))
        xy_lo = lower(np.array([ndc_lo[0], -ndc_hi[1]]) + 1)
        xy_hi = upper(np.array([ndc_hi[0], -ndc_lo[1]]) + 1)
        pixel_lo = lower(lower(xy_lo * raster) / 2)
        pixel_hi = upper(upper(xy_hi * raster) / 2)
    if not np.isfinite(pixel_lo).all() or not np.isfinite(pixel_hi).all():
        return None
    return pixel_lo, pixel_hi


def potential_samples(source_bounds, world, points, raster):
    bounds = screen_bounds(source_bounds, world, raster)
    if bounds is None:
        return np.ones(len(points), bool), False
    return ((points >= bounds[0]) & (points <= bounds[1])).all(1), True
