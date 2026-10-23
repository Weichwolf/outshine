"""Coordinate CDF strata with one pixel-row band and a bounded ray packet."""

import numpy as np


def pixel_band(width, spatial, draws, seed, row):
    rng = np.random.default_rng(np.random.SeedSequence(seed, spawn_key=(2, row)))
    shape = (width, draws, spatial**2)
    rank = rng.permuted(np.broadcast_to(np.arange(spatial**2), shape), axis=-1)
    uniform = stratum_uniforms(rank, rng.random(shape), spatial, draws)
    del rank
    return raster_band(uniform, width, spatial, draws)


def stratum_uniforms(rank, uniform, spatial, draws):
    uniform += rank
    uniform /= spatial**2
    uniform += np.arange(draws)[None, :, None]
    uniform /= draws
    np.minimum(uniform, np.nextafter(1.0, 0), out=uniform)
    return uniform


def raster_band(uniform, width, spatial, draws):
    return (
        uniform.transpose(0, 2, 1)
        .reshape(width, spatial, spatial, draws)
        .transpose(1, 0, 2, 3)
        .reshape(-1, draws)
    )


def screen_band(width, spatial, draws, seed, first_column, row):
    """Fixed-size Philox blocks per pixel; keys replace variable shuffles.

    Ideal continuous keys give a uniform permutation. Equal uint64 keys use
    stable index order; finite PRNG/tie/rounding bias is not certified.
    """
    lanes = draws * spatial**2
    blocks = (2 * lanes + 3) // 4
    generator = np.random.Philox(
        np.random.SeedSequence(seed, spawn_key=(3,)),
        counter=[first_column * blocks, row, spatial, draws],
    )
    raw = generator.random_raw(width * blocks * 4).reshape(width, blocks * 4)
    rank = np.argsort(
        raw[:, :lanes].reshape(width, draws, spatial**2), axis=-1, kind="stable"
    )
    jitter = raw[:, lanes : 2 * lanes].copy()
    del raw
    jitter >>= 11
    uniform = jitter.astype(float).reshape(width, draws, spatial**2)
    del jitter
    uniform *= 1 / 2**53
    uniform = stratum_uniforms(rank, uniform, spatial, draws)
    del rank
    return raster_band(uniform, width, spatial, draws)


def pixel_surface_packets(
    width, height, spatial, draws, seed, *, batch=2048, pixel_origin=None
):
    """Yield image_rays raster-order uniforms, valid until the next iteration.

    Each ray retains M strata. Within each stratum a random permutation assigns
    P spatial rays to P subintervals, with independent jitter. A fixed ray's
    marginal stays uniform in its stratum; pixel variance can improve or worsen.
    Row-indexed streams keep results independent of query packet size. Optional
    uint32 screen pixel_origin makes windows and tiles share per-pixel streams.
    This does not reproject history or define a persistent random world.
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
    if pixel_origin is not None and (
        not isinstance(pixel_origin, (tuple, list))
        or len(pixel_origin) != 2
        or any(type(v) is not int or v < 0 for v in pixel_origin)
        or pixel_origin[0] + width > 2**32
        or pixel_origin[1] + height > 2**32
    ):
        raise ValueError("uint32 screen pixel rectangle required")
    packet = np.empty((batch, draws))
    filled = 0
    for row in range(height):
        band = (
            pixel_band(width, spatial, draws, seed, row)
            if pixel_origin is None
            else screen_band(
                width, spatial, draws, seed, pixel_origin[0], pixel_origin[1] + row
            )
        )
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
