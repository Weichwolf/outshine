"""Complete arrival includes actual facade parts and rejects a raised threshold."""

from dataclasses import replace
import unittest

import numpy as np

from building_layout import rectangle
from building_mesh import solid
from building_plan import FLOOR_M
from terraced_facade_access import check_arrival
from terraced_facade_geometry import build_facade
from terraced_mass_plan import plan
from terraced_mass_sources import sources


def threshold(mass, rise):
    storage = [], [], [], []
    region = rectangle(mass.entry.centre, mass.entry.tangent, 1.1, 0.10, 0.04)
    floor = mass.bottom_m + FLOOR_M
    solid(storage, region, floor - 0.05, floor + rise, 9)
    return {
        "vertices": np.asarray(storage[0], dtype=np.float64),
        "faces": np.asarray(storage[1], dtype=np.uint32),
        "materials": np.asarray(storage[2], dtype=np.uint16),
        "parts": np.asarray(storage[3], dtype=np.int32),
    }


class FacadeArrivalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.products = [
            (p, build_facade(p))
            for row in sources()
            for kind in ("cascade", "paired-wings")
            for p in [plan(row, kind)]
        ]

    def test_all_actual_arrivals_include_facade_parts_and_clear_lower_legs(self):
        for p, asset in self.products:
            result = check_arrival(p, asset)
            self.assertTrue(result["continuous_lower_legs_checked"])
            self.assertAlmostEqual(result["path_length_m"], 2.3)

    def test_a_four_centimetre_raised_threshold_fails_the_physical_leg_gate(self):
        p, asset = self.products[0]
        blocked = replace(asset, frames=threshold(p, 0.04))
        with self.assertRaisesRegex(ValueError, "continuous lower-leg/foot tube"):
            check_arrival(p, blocked)

    def test_a_flush_threshold_is_included_and_remains_traversable(self):
        p, asset = self.products[0]
        flush = replace(asset, frames=threshold(p, 0))
        result = check_arrival(p, flush)
        self.assertEqual(result["additional_closed_facade_parts"], 1)


if __name__ == "__main__":
    unittest.main()
