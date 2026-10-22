"""Independent analytic controls and native-result classification for #829."""

import math
import struct
import unittest
from dataclasses import replace

from ggx_stability import (
    FLOAT_MAX,
    FLOAT_MIN,
    Case,
    compare,
    fixtures,
    from_bits,
    reference,
)


def sample(**changes):
    base = Case("control", "mirror", "dielectric", 1, 1e-8, 1, 1, 1, (0.04,) * 3, 1)
    return replace(base, **changes)


def raw(values):
    return struct.pack("<8f", *values)


class GgxStability(unittest.TestCase):
    def test_analytic_normal_incidence_peak(self):
        for moment in (1, 0.5**4, 0.01**4, FLOAT_MIN):
            d, visibility, lobe, red, *_ = reference(sample(a2=moment))
            self.assertAlmostEqual(d * math.pi * moment, 1, places=12)
            self.assertEqual(visibility, 0.25)
            self.assertAlmostEqual(lobe / d, 0.25, places=12)
            self.assertAlmostEqual(red / lobe, 0.04, places=12)

    def test_unit_moment_has_constant_normal_distribution(self):
        for cosine in (0.1, 0.5, 1):
            d = reference(sample(a2=1, nh=cosine))[0]
            self.assertAlmostEqual(d, 1 / math.pi, places=12)

    def test_zero_moment_preserves_existing_brdf_endpoint(self):
        values = reference(sample(a2=0))
        self.assertEqual(values, (0,) * 8)

    def test_scalar_overflow_can_hide_representable_rgb(self):
        case = sample(a2=FLOAT_MIN, nl=0.14, nv=0.14, vh=0.14)
        _, _, lobe, red, *_ = reference(case)
        self.assertGreater(lobe, FLOAT_MAX)
        self.assertGreater(red, 0)
        self.assertLess(red, FLOAT_MAX)

    def test_disabled_fresnel_stays_zero_at_the_same_boundary(self):
        values = reference(sample(a2=FLOAT_MIN, nl=0.14, nv=0.14, f0=(0,) * 3, f90=0))
        self.assertGreater(values[2], FLOAT_MAX)
        self.assertEqual(values[3:], (0,) * 5)

    def test_grazing_radiance_can_be_representable_after_rgb_overflow(self):
        case = sample(a2=1e-32, nl=1e-6, nv=1e-6, vh=1e-6)
        unweighted = reference(case)
        weighted = reference(replace(case, cosine_weighted=True))
        self.assertGreater(unweighted[3], FLOAT_MAX)
        self.assertLess(weighted[3], FLOAT_MAX)
        self.assertAlmostEqual(weighted[3] / unweighted[3], case.nl, places=15)

    def test_premature_saturation_before_cosine_is_rejected(self):
        case = sample(a2=1e-32, nl=1e-6, nv=1e-6, vh=1e-6, cosine_weighted=True)
        values = list(reference(case))
        values[2] = FLOAT_MAX
        values[3:] = (FLOAT_MAX * case.nl,) * 4 + (0,)
        result = compare([case], raw(values))
        self.assertEqual(len(result["normal_input_errors"]), 4)

    def test_rounded_cosines_do_not_produce_negative_colours(self):
        values = reference(
            sample(nh=from_bits(0x3F800001), vh=from_bits(0x3F800001), f0=(0, 0.04, 1))
        )
        self.assertEqual(values[4], 0)
        self.assertGreater(values[5], 0)
        self.assertGreater(values[6], values[5])

    def test_nonfinite_representable_results_fail(self):
        case = sample()
        values = list(reference(case))
        values[3] = math.inf
        result = compare([case], raw(values))
        self.assertEqual(len(result["nonfinite_representable"]), 1)
        self.assertEqual(result["normal_input_errors"], [])

    def test_inaccurate_finite_normal_input_fails(self):
        case = sample()
        values = list(reference(case))
        values[3] = 0
        result = compare([case], raw(values))
        self.assertEqual(len(result["normal_input_errors"]), 1)

    def test_input_denorm_losses_are_retained_separately(self):
        result = compare([sample(a2=1e-40)], raw((0,) * 8))
        self.assertGreater(len(result["input_denorm_accuracy_losses"]), 0)
        self.assertEqual(result["normal_input_errors"], [])
        self.assertEqual(result["nonfinite_representable"], [])

    def test_native_matrix_keeps_distinct_boundary_classes(self):
        cases = fixtures()
        self.assertTrue(any(c.moment == "minimum-normal" for c in cases))
        self.assertTrue(any(c.moment == "subnormal" for c in cases))
        self.assertTrue(any(c.pose == "asymmetric-grazing" for c in cases))
        self.assertTrue(any(c.pose == "rounded-fresnel-cosine" for c in cases))
        self.assertTrue(any(c.fresnel == "disabled" for c in cases))
        self.assertTrue(all(len(c.fields()) == 12 for c in cases))


if __name__ == "__main__":
    unittest.main()
