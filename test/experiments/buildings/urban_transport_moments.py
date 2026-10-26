"""Current-view joint radiance moments; no lighting is baked into source assets."""

import numpy as np


def pack_moments(colour, transmission, visibility, direct, other):
    """Pack C, attenuated B, B and T, preserving joint direct/ambient transport."""
    c, d = np.asarray(colour, float), np.asarray(direct, float)
    t, s = np.asarray(transmission, float), np.asarray(visibility, float)
    if c.ndim != 2 or c.shape[1:] != (3,) or d.shape != c.shape:
        raise ValueError("pointwise RGB arrays required")
    if t.shape != c.shape[:1] or s.shape != t.shape:
        raise ValueError("one T and S per point required")
    o = np.broadcast_to(np.asarray(other, float), c.shape)
    if not (
        all(np.isfinite(a).all() and np.all(a >= 0) for a in [c, d, o, t, s])
        and np.all(t <= 1)
        and np.all(s <= 1)
    ):
        raise ValueError("finite nonnegative radiance and T/S in [0,1] required")
    background = d + o
    received = t[:, None] * (s[:, None] * d + o)
    return np.column_stack((c, received, background, t))


def compose_moments(moments, background):
    """Normalize joint received radiance by sampled B, then apply denser B.

    Zero sampled B has no lighting evidence; mean T is a bounded approximation.
    The ratio/fallback has no unbiasedness or error-bound guarantee. Same-grid
    means recover C+R; full readiness recovers any dense B, including black cases.
    """
    m, b = np.asarray(moments, float), np.asarray(background, float)
    if m.shape[-1:] != (10,) or b.shape != (*m.shape[:-1], 3):
        raise ValueError("ten filtered fields and matching RGB background required")
    if not (
        np.isfinite(m).all()
        and np.isfinite(b).all()
        and np.all(m >= 0)
        and np.all(b >= 0)
    ):
        raise ValueError("finite nonnegative moments and background required")
    c, received, coarse = m[..., :3], m[..., 3:6], m[..., 6:9]
    if np.any(received > coarse) or np.any(m[..., 9] > 1):
        raise ValueError("joint received <= sampled B and mean T <= 1 required")
    fallback = coarse == 0
    transport = np.broadcast_to(m[..., 9, None], b.shape).copy()
    np.divide(received, coarse, out=transport, where=~fallback)
    rgb = c + transport * b
    if not np.isfinite(rgb).all():
        raise ValueError("finite composed RGB required")
    return rgb, transport, int(np.count_nonzero(fallback))
