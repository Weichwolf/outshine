"""Prefix intervals against individually intersected original categorical boxes."""

import unittest

import numpy as np
from test_urban_parent_phase import homogeneous_parent
from urban_parent_conditional import density, grid
from urban_parent_prefix import AtomicPrefix


def enumerated_prefix(parent, origin, direction, stop):
    product = grid(parent)
    x, y = np.meshgrid(np.arange(product["shape"][0]), np.arange(product["shape"][1]))
    cells = np.c_[x.ravel(), y.ravel()]
    pitch = product["header"]["pitch_m"]
    centres = product["reference_origin_m"][:2] + (cells + 0.5) * pitch
    half = pitch * parent.frame[5] / 2
    probabilities = np.zeros(len(cells))
    for height, mass in zip(parent.heights, parent.probabilities, strict=True):
        low = np.c_[centres - half, np.zeros(len(cells))]
        high = np.c_[centres + half, np.full(len(cells), height)]
        parallel = direction == 0
        t = np.divide(
            np.stack((low, high), axis=1) - origin,
            direction,
            out=np.zeros((len(cells), 2, 3)),
            where=~parallel,
        )
        entry = np.where(parallel, -np.inf, t.min(axis=1)).max(axis=1)
        leave = np.where(parallel, np.inf, t.max(axis=1)).min(axis=1)
        outside = np.any(parallel & ((origin < low) | (origin > high)), axis=1)
        hit = ~outside & (np.maximum(entry, 0.1) < np.minimum(leave, stop))
        probabilities += mass * hit
    return float(np.prod(1 - density(parent, product, cells) * probabilities))


class ParentPrefixTest(unittest.TestCase):
    def test_corridors_and_uniform_occupancy_match_box_enumeration(self):
        parent = homogeneous_parent()
        parent.frame[3] = 128
        model = AtomicPrefix(parent)
        for x in [8.0, 15.68, 2.0, 14.0]:
            origin = np.array([x, -16.0, 2])
            direction = np.array([0.0, 1, 0])
            interval = model.interval(origin, direction)
            exact = enumerated_prefix(parent, origin, direction, np.inf)
            self.assertLessEqual(interval[0], exact + 2e-14)
            self.assertGreaterEqual(interval[1], exact - 2e-14)
            if x == 15.68:
                np.testing.assert_array_equal(interval, [1, 1])
            elif x == 8:
                np.testing.assert_allclose(interval, 0.15**8, atol=2e-14, rtol=0)

    def test_nonuniform_slanted_prefixes_enclose_independent_box_expectations(self):
        rng = np.random.default_rng(1113)
        parent = homogeneous_parent()
        parent.centre_fractions = rng.random((4, 4, 3))
        model = AtomicPrefix(parent)
        for _ in range(70):
            origin = np.array([rng.uniform(-128, 640), -128.0, rng.uniform(1, 140)])
            target = np.array(
                [rng.uniform(0, 512), rng.uniform(0, 512), rng.uniform(0, 48)]
            )
            direction = target - origin
            direction /= np.linalg.norm(direction)
            stop = rng.choice([np.linalg.norm(target - origin), np.inf])
            interval = model.interval(origin, direction, stop)
            exact = enumerated_prefix(parent, origin, direction, stop)
            self.assertLessEqual(interval[0], exact + 2e-13)
            self.assertGreaterEqual(interval[1], exact - 2e-13)

    def test_invalid_prefix_is_rejected(self):
        model = AtomicPrefix(homogeneous_parent())
        for origin, direction, stop in [
            ([0, 0, -1], [0, 1, 0], 100),
            ([0, 0, 1], [0, 0, 0], 100),
            ([0, 0, 1], [0, 1, 0], np.nan),
        ]:
            with self.assertRaisesRegex(ValueError, "geometry"):
                model.interval(origin, direction, stop)


if __name__ == "__main__":
    unittest.main()
