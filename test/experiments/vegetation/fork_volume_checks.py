"""Independent enclosure probes and exact dense/blocked crossing-cell comparisons."""

from itertools import product

import numpy as np
from fork_field import ForkField
from fork_intervals import block_intervals
from fork_surface import blocked_voxel_mesh, voxel_mesh
from fork_volume import blocked_volume, dense_volume


def enclosure_checks():
    rng = np.random.default_rng(2352)
    tested = 0
    corners = np.asarray(list(product((0.0, 1.0), repeat=3)))
    for count in (1, 3, 16):
        for scale in (0.001, 1.0, 1000.0):
            for blend in (0.0, 1e-12, 0.035):
                endpoints = rng.uniform(-1, 1, (count, 3)) * scale
                endpoints[0] = 0
                field = ForkField(
                    endpoints,
                    rng.uniform(0.05, 0.3, count) * scale,
                    blend * scale**2,
                    (1e6, -1e6, 1e6),
                )
                minima = field.origin + rng.uniform(-2, 2, (32, 3)) * scale
                maxima = minima + rng.uniform(0, 0.5, (32, 3)) * scale
                low, high = block_intervals(field, minima, maxima)
                fractions = np.vstack((corners, rng.random((32, 3))))
                points = minima[:, None] + fractions * (maxima - minima)[:, None]
                values = field.evaluate(points.reshape(-1, 3)).reshape(32, -1)
                assert np.all(values >= low[:, None])
                assert np.all(values <= high[:, None])
                tested += values.size
    return {"enclosed_point_probes": tested, "box_count": 27 * 32}


def crossing_corners(volume):
    shape = tuple(v - 1 for v in volume.shape)
    low = np.full(shape, np.inf)
    high = np.full(shape, -np.inf)
    offsets = list(product((0, 1), repeat=3))
    for offset in offsets:
        region = tuple(slice(o, o + n) for o, n in zip(offset, shape, strict=True))
        low = np.minimum(low, volume[region])
        high = np.maximum(high, volume[region])
    crossing = (low <= 0) & (high >= 0)
    required = np.zeros(volume.shape, dtype=bool)
    for offset in offsets:
        region = tuple(slice(o, o + n) for o, n in zip(offset, shape, strict=True))
        required[region] |= crossing
    return required


def equivalence_checks(fields):
    cases = 0
    for field in fields.values():
        for cells in (16, 32):
            dense, origin, step, _work = dense_volume(field, cells)
            required = crossing_corners(dense)
            reference, _work = voxel_mesh(field, cells)
            for block_size in (2, 4, 8, 16):
                blocked, actual_origin, actual_step, _work = blocked_volume(
                    field, cells, block_size
                )
                np.testing.assert_array_equal(origin, actual_origin)
                assert step == actual_step
                np.testing.assert_array_equal(np.sign(dense), np.sign(blocked))
                np.testing.assert_array_equal(dense[required], blocked[required])
                mesh, _work = blocked_voxel_mesh(field, cells, block_size)
                for key in reference:
                    np.testing.assert_array_equal(reference[key], mesh[key])
                cases += 1
    return {"exact_dense_blocked_mesh_cases": cases}


if __name__ == "__main__":
    import json

    from fork_experiment import fixtures

    print(json.dumps(enclosure_checks() | equivalence_checks(fixtures())))
