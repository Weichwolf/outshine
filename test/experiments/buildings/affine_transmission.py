"""Log products on affine density runs; no per-cell work for long runs.

Positive real gamma recurrence/Stirling expansion, NIST DLMF5.11. Floating
evaluation is measured against enumerated controls, not a directed-rounding bound.
"""

import numpy as np


def log1pmx(value):
    result = np.log1p(value) - value
    small = value < 0.125
    v = value[small]
    term = v * v
    series = -term / 2
    for power in range(3, 22):
        term *= v
        series += (-1 if power % 2 == 0 else 1) * term / power
    result[small] = series
    return result


def stirling_correction(value):
    inverse = 1 / value
    square = inverse * inverse
    return inverse * (
        1 / 12
        + square
        * (
            -1 / 360
            + square
            * (
                1 / 1260
                + square * (-1 / 1680 + square * (1 / 1188 - square * 691 / 360360))
            )
        )
    )


def sum_log1m_affine(first, last, count, tail):
    """Sum log(1-tail*q[i]) for linearly interpolated q, 0<=q<=.85."""
    first, last, count = np.broadcast_arrays(
        np.asarray(first, float), np.asarray(last, float), np.asarray(count, np.int64)
    )
    shape = first.shape
    first, last, count = first.ravel(), last.ravel(), count.ravel()
    if (
        not np.isfinite(first).all()
        or not np.isfinite(last).all()
        or np.any(first < 0)
        or np.any(last < 0)
        or np.any(first > 0.85 + 1e-14)
        or np.any(last > 0.85 + 1e-14)
        or not np.isfinite(tail)
        or not 0 <= tail <= 1
        or np.any(count < 0)
        or np.any(count > 2**31)
    ):
        raise ValueError("bounded nonnegative affine density required")
    result = np.zeros(len(first))
    if tail == 0:
        return result.reshape(shape)
    short = np.flatnonzero((count > 0) & (count <= 16))
    for i in range(16):
        at = short[count[short] > i]
        fraction = i / np.maximum(count[at] - 1, 1)
        q = first[at] + fraction * (last[at] - first[at])
        result[at] += np.log1p(-tail * q)
    long = np.flatnonzero(count > 16)
    if not len(long):
        return result.reshape(shape)
    q0, q1, n = first[long], last[long], count[long].astype(float)
    midpoint = (q0 + q1) / 2
    mid_void = 1 - tail * midpoint
    step = tail * np.abs(q1 - q0) / (n - 1)
    flat = step * n / mid_void < 1e-8
    result[long[flat]] = (
        n[flat] * np.log1p(-tail * midpoint[flat])
        - (step[flat] / mid_void[flat]) ** 2 * n[flat] * (n[flat] ** 2 - 1) / 24
    )
    use = ~flat
    ids, n, step = long[use], n[use], step[use]
    q = np.maximum(q0[use], q1[use])
    prefix = np.zeros(len(ids))
    for _ in range(8):
        early = (1 - tail * q) / step < 8
        prefix[early] += np.log1p(-tail * q[early])
        q[early] -= step[early] / tail
        n[early] -= 1
    x = (1 - tail * q) / step
    ratio = n / x
    result[ids] = (
        prefix
        + n * np.log1p(-tail * q)
        + x * log1pmx(ratio)
        + (n - 0.5) * np.log1p(ratio)
        + stirling_correction(x + n)
        - stirling_correction(x)
    )
    return result.reshape(shape)
