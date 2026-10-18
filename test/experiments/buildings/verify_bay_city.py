"""Actual whole-source addition checker and blocked-opening/detached-wall negatives."""

import unittest
from dataclasses import replace

import numpy as np
import shapely
from bay_city import check_prefix
from bay_city_checks import inspect
from bay_facade_controls import cases
from bay_facade_plan import PROGRAMMES, plan_bays


class BayCityChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.row, _label, cls.terrace = cases()[1]
        cls.plans = [
            plan_bays(cls.terrace.prepared, style, portals=cls.terrace.portals)
            for style in PROGRAMMES
        ]

    def test_actual_closed_additions_keep_openings_wall_attachment_and_routes(self):
        for planned in self.plans:
            with self.subTest(programme=planned.programme):
                result = inspect(planned, self.terrace)
                self.assertGreater(result["closed_layers"], 0)
                self.assertGreater(result["actual_opening_rays"], 0)
                self.assertGreater(result["source_wall_back_samples"], 0)
                self.assertLessEqual(result["maximum_volume_error_m3"], 1e-7)

    def changed(self, layer):
        planned = self.plans[1]
        cell = planned.cells[0]
        return replace(planned, cells=(replace(cell, layers=(layer,)),))

    def test_valid_closed_rectangle_over_actual_opening_is_rejected(self):
        layer = self.plans[1].cells[0].layers[0]
        # Filling the real protected window holes is a valid positive solid.
        filled = shapely.box(*layer.region.bounds)
        bad = replace(layer, region_wkb=filled.wkb, protected_wkb=shapely.Polygon().wkb)
        with self.assertRaisesRegex(ValueError, "source opening"):
            inspect(self.changed(bad), self.terrace)

    def test_finite_closed_but_detached_back_surface_is_rejected(self):
        layer = self.plans[1].cells[0].layers[0]
        bad = replace(layer, origin_xy=tuple(x + 10000.0 for x in layer.origin_xy))
        with self.assertRaisesRegex(ValueError, "source wall"):
            inspect(self.changed(bad), self.terrace)

    def test_existing_draw_prefix_is_byte_exact_including_signed_zero_and_dtype(self):
        original = {"vertices": np.array([[0.0, 1.0, 2.0]])}
        joined = {"vertices": np.array([[0.0, 1.0, 2.0], [3.0, 4.0, 5.0]])}
        check_prefix(original, joined)
        joined["vertices"][0, 0] = -0.0
        with self.assertRaisesRegex(ValueError, "numeric arrays"):
            check_prefix(original, joined)
        with self.assertRaisesRegex(ValueError, "array layout"):
            check_prefix(
                original, {"vertices": original["vertices"].astype(np.float32)}
            )


if __name__ == "__main__":
    unittest.main()
