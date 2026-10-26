"""Bounded reference pixel integration; camera and complete scene are callbacks."""

import numpy as np


def pixel_packets(box, ray_query, samples, seed, batch):
    """Yield reusable ray/owner buffers; storage lasts until next iteration."""
    origins = np.empty((batch, 3))
    directions = np.empty((batch, 3))
    owners = np.empty(batch, np.int64)
    filled = 0
    width = int(box[1, 0] - box[0, 0])
    for y in range(int(box[0, 1]), int(box[1, 1])):
        for x in range(int(box[0, 0]), int(box[1, 0])):
            stream = (
                None
                if seed is None
                else np.random.SeedSequence(seed, spawn_key=(4, x, y))
            )
            o, d = ray_query((x, y), samples, stream)
            o, d = np.asarray(o, float), np.asarray(d, float)
            if (
                o.shape != (samples**2, 3)
                or d.shape != o.shape
                or not (np.isfinite(o).all() and np.isfinite(d).all())
            ):
                raise ValueError("one finite ray per spatial stratum required")
            with np.errstate(over="ignore", invalid="ignore"):
                norm = np.linalg.norm(d, axis=1)
            if not np.all(np.isfinite(norm) & (norm > 0)):
                raise ValueError("finite positive ray lengths required")
            d = d / norm[:, None]
            owner = (y - int(box[0, 1])) * width + x - int(box[0, 0])
            first = 0
            while first < len(o):
                count = min(batch - filled, len(o) - first)
                origins[filled : filled + count] = o[first : first + count]
                directions[filled : filled + count] = d[first : first + count]
                owners[filled : filled + count] = owner
                filled += count
                first += count
                if filled == batch:
                    yield origins, directions, owners
                    filled = 0
    if filled:
        yield origins[:filled], directions[:filled], owners[:filled]


def integrate_pixels(box, ray_query, scene, *, samples=16, seed=None, batch=2048):
    """Average complete linear RGB and coverage, without clamping signed values."""
    return integrate_fields(
        box, ray_query, scene, channels=3, samples=samples, seed=seed, batch=batch
    )


def integrate_fields(
    box, ray_query, scene, *, channels, samples=16, seed=None, batch=2048
):
    """Average bounded pointwise fields; physical products belong in the callback.

    Camera receives (absolute pixel, samples per axis, per-pixel SeedSequence).
    None seed selects fixed strata, otherwise the camera jitters each stratum.
    Scene receives unit rays and returns complete fields plus coverage. Joint
    transport fields must already contain their T*S/background products.
    Memory follows output pixels, one pixel's rays and one bounded scene packet.
    """
    raw = np.asarray(box)
    if (
        raw.shape != (2, 2)
        or raw.dtype.kind not in "iu"
        or np.any(raw < 0)
        or np.any(raw > 2**32)
    ):
        raise ValueError("uint32 pixel rectangle required")
    box = raw.astype(np.int64)
    if np.any(box[1] < box[0]) or not (
        type(samples) is int
        and 1 <= samples <= 128
        and type(channels) is int
        and 1 <= channels <= 32
        and type(batch) is int
        and 1 <= batch <= 8192
        and (seed is None or type(seed) is int and seed >= 0)
    ):
        raise ValueError(
            "ordered pixel rectangle, bounded samples/packet and optional seed required"
        )
    width, height = map(int, box[1] - box[0])
    rgb = np.zeros((width * height, channels))
    coverage = np.zeros(width * height)
    work = {
        "rays": 0,
        "packets": 0,
        "max_packet_rays": 0,
        "output_array_B": rgb.nbytes + coverage.nbytes,
        "packet_capacity_B": batch * 56 if width * height else 0,
        "per_pixel_camera_output_logical_B": samples**2 * 48 if width * height else 0,
        "camera_to_packet_copy_B": 0,
        "owner_index_write_B": 0,
        "value_channels": channels,
        "scene_value_logical_B": 0,
    }
    if width * height:
        for origins, directions, owners in pixel_packets(
            box, ray_query, samples, seed, batch
        ):
            colours, alpha = scene(origins, directions)
            colours, alpha = np.asarray(colours, float), np.asarray(alpha, float)
            if (
                colours.shape != (len(owners), channels)
                or alpha.shape != owners.shape
                or not (
                    np.isfinite(colours).all()
                    and np.all(np.isfinite(alpha) & (alpha >= 0) & (alpha <= 1))
                )
            ):
                raise ValueError(
                    "finite pointwise fields and coverage in [0,1] required"
                )
            for channel in range(channels):
                np.add.at(rgb[:, channel], owners, colours[:, channel])
            np.add.at(coverage, owners, alpha)
            work["rays"] += len(owners)
            work["packets"] += 1
            work["max_packet_rays"] = max(work["max_packet_rays"], len(owners))
            work["camera_to_packet_copy_B"] += len(owners) * 48
            work["owner_index_write_B"] += len(owners) * 8
            work["scene_value_logical_B"] += colours.nbytes
    rgb /= samples**2
    coverage /= samples**2
    return rgb.reshape(height, width, channels), coverage.reshape(height, width), work
