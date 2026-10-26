"""Analytic mask integrals, holes/components and complete-domain refusal."""

import unittest

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from test_coarse_water_owner import square
from water_field_difference import compare_fields


def field(size, up):
    mesh = square(np.full(4, up))
    vertices = mesh.vertices.copy()
    vertices[:, :2] *= size
    return Mesh(vertices, mesh.triangles)


class MaskedWaterDifference(unittest.TestCase):
    def test_different_owner_domains_require_an_explicit_complete_mask(self):
        first, second = field(2, 501), field(3, 500)
        with self.assertRaisesRegex(ValueError, "complete XY domain"):
            compare_fields(first, second)
        mask = shapely.Polygon(
            [(0, 0), (2, 0), (2, 2), (0, 2)],
            [[(0.5, 0.5), (1.5, 0.5), (1.5, 1.5), (0.5, 1.5)]],
        )
        result = compare_fields(first, second, domain_mask=mask).report
        self.assertEqual(result["comparison_domain"], "explicit_water_mask")
        self.assertAlmostEqual(result["overlay_area_m2"], 3)
        self.assertAlmostEqual(result["maximum_enu_up_difference_m"], 1)
        self.assertAlmostEqual(result["integrated_enu_up_rms_m"], 1)
        self.assertEqual(result["first_mask_missing_area_m2"], 0)
        self.assertEqual(result["second_mask_missing_area_m2"], 0)

    def test_disconnected_nonconvex_mask_retains_all_area(self):
        mask = shapely.union_all(
            [
                shapely.box(0, 0, 1, 0.5),
                shapely.box(0, 0, 0.5, 1),
                shapely.box(1.5, 1.5, 2, 2),
            ]
        )
        result = compare_fields(field(2, 502), field(3, 500), domain_mask=mask).report
        self.assertAlmostEqual(result["overlay_area_m2"], 1)
        self.assertAlmostEqual(result["maximum_enu_up_difference_m"], 2)
        self.assertAlmostEqual(result["integrated_enu_up_rms_m"], 2)

    def test_affine_mask_integral_is_independent_of_cdt_triangulation(self):
        # e=x: integral=1/3-(1/2)*((3/4)^3-(1/4)^3)/3=17/64, area=3/4.
        mask = shapely.box(0, 0, 1, 1).difference(shapely.box(0.25, 0.25, 0.75, 0.75))
        first = square([500, 501, 501, 500])
        second = square([500] * 4, True)
        result = compare_fields(first, second, domain_mask=mask).report
        self.assertAlmostEqual(result["overlay_area_m2"], 0.75)
        self.assertAlmostEqual(result["maximum_enu_up_difference_m"], 1)
        self.assertAlmostEqual(result["integrated_enu_up_rms_m"], np.sqrt(17 / 48))
        self.assertAlmostEqual(result["mean_enu_up_difference_m"], 0.5)

    def test_missing_mask_invalid_geometry_and_work_caps_refuse(self):
        mask = shapely.box(0, 0, 2, 2)
        with self.assertRaisesRegex(ValueError, "complete declared mask"):
            compare_fields(field(1, 500), field(2, 500), domain_mask=mask)
        for invalid in (shapely.Point(0, 0), shapely.Polygon()):
            with self.assertRaisesRegex(ValueError, "valid polygonal"):
                compare_fields(field(2, 500), field(2, 500), domain_mask=invalid)
        with self.assertRaisesRegex(ValueError, "overlay-fan budget"):
            compare_fields(field(2, 500), field(2, 500), domain_mask=mask, fan_limit=1)


if __name__ == "__main__":
    unittest.main()
