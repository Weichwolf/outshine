"""Complementary whole-cut sample ownership, distinct from independent alpha layers."""

import numpy as np


def ownership_ranks(width, height, rate=16, *, granularity="sample", seed=0):
    if (
        not all(isinstance(v, int) and v > 0 for v in (width, height, rate))
        or rate > 16
        or width * height * rate**2 > 1048576
        or not isinstance(seed, int)
        or not 0 <= seed < 2**64
        or granularity not in ("sample", "pixel")
    ):
        raise ValueError("bounded positive quadrature shape and explicit seed required")
    samples = rate**2
    rng = np.random.Generator(np.random.PCG64(seed))
    if granularity == "pixel":
        ranks = rng.permutation(width * height).reshape(height, width)
        return (
            ranks.repeat(rate, axis=0).repeat(rate, axis=1).astype(np.uint32).ravel(),
            width * height,
        )
    ranks = np.array(
        [rng.permutation(samples) for _ in range(width * height)], np.uint32
    )
    return ranks.reshape(height, width, rate, rate).transpose(
        0, 2, 1, 3
    ).ravel(), samples


def owned_masks(ranks, stratum_size, weight):
    ranks = np.asarray(ranks)
    if (
        ranks.ndim != 1
        or ranks.dtype != np.uint32
        or not isinstance(stratum_size, int)
        or not 1 <= stratum_size <= 1048576
        or np.any(ranks >= stratum_size)
        or not np.isfinite(weight)
        or not 0 <= weight <= 1
    ):
        raise ValueError("bounded sample ranks and weight in[0,1] required")
    selected = int(np.floor(weight * stratum_size + 0.5))
    coarser = ranks < selected
    return ~coarser, coarser, selected / stratum_size


def sum_owned(first, second):
    if any(first[k].shape != second[k].shape for k in ("C", "T")):
        raise ValueError("matching partial sample measures required")
    return {key: first[key] + second[key] for key in ("C", "T")}
