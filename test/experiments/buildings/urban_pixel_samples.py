"""Coordinate CDF strata with one pixel-row band and a bounded ray packet."""

import numpy as np


def pixel_band(width, spatial, draws, seed, row):
    rng = np.random.default_rng(np.random.SeedSequence(seed, spawn_key=(2, row)))
    shape = (width, draws, spatial**2)
    rank = rng.permuted(np.broadcast_to(np.arange(spatial**2), shape), axis=-1)
    uniform = rng.random(shape)
    uniform += rank
    uniform /= spatial**2
    uniform += np.arange(draws)[None, :, None]
    uniform /= draws
    np.minimum(uniform, np.nextafter(1.0, 0), out=uniform)
    del rank
    return (
        uniform.transpose(0, 2, 1)
        .reshape(width, spatial, spatial, draws)
        .transpose(1, 0, 2, 3)
        .reshape(-1, draws)
    )


def pixel_surface_packets(width, height, spatial, draws, seed, *, batch=2048):
    """Yield image_rays raster-order uniforms, valid until the next iteration.

    Each ray retains M strata. Within each stratum a random permutation assigns
    P spatial rays to P subintervals, with independent jitter. A fixed ray's
    marginal stays uniform in its stratum; pixel variance can improve or worsen.
    Row-indexed streams keep results independent of query packet size. This does
    not reproject history, smooth across pixels or define a persistent world.
    """
    if any(type(v) is not int or v < 1 for v in (width, height)) or not (
        type(spatial) is int
        and 1 <= spatial <= 16
        and type(draws) is int
        and 1 <= draws <= 512
        and type(batch) is int
        and 1 <= batch <= 2048
    ):
        raise ValueError(
            "positive image size and bounded sample/packet counts required"
        )
    packet = np.empty((batch, draws))
    filled = 0
    for row in range(height):
        band = pixel_band(width, spatial, draws, seed, row)
        first = 0
        while first < len(band):
            count = min(batch - filled, len(band) - first)
            packet[filled : filled + count] = band[first : first + count]
            filled += count
            first += count
            if filled == batch:
                yield packet
                filled = 0
        del band
    if filled:
        yield packet[:filled]
