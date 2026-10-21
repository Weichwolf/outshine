"""Closed forms and independently bounded failure cases for the #829 oracle."""

import unittest
from decimal import Decimal, localcontext

import numpy as np
from ggx_reference import PI, compare, decimal, evaluate, fixtures, row_reference


class GgxReferenceTests(unittest.TestCase):
    def test_normal_incidence_closed_form_even_below_float32_range(self):
        with localcontext() as context:
            context.prec = 100
            for q in (1, 1e-8, 1e-20, np.finfo(np.float32).smallest_subnormal):
                d, v, lobe, spec = evaluate(q, 1, 1, 1, 0.04)
                exact_d = 1 / (PI * decimal(q))
                self.assertLess(abs(d / exact_d - 1), Decimal("1e-50"))
                self.assertEqual(v, Decimal("0.25"))
                self.assertLess(abs(lobe / (d / 4) - 1), Decimal("1e-50"))
                self.assertLess(
                    abs(spec / (exact_d * decimal(0.04) / 4) - 1), Decimal("1e-50")
                )

    def test_smith_visibility_at_unit_roughness(self):
        with localcontext() as context:
            context.prec = 100
            for nl, nv in ((0.5, 1), (1e-6, 0.25)):
                _, v, _, _ = evaluate(1, nl, nv, 0.5)
                self.assertLess(
                    abs(v * (2 * (decimal(nl) + decimal(nv))) - 1), Decimal("1e-50")
                )

    def test_neutral_fresnel_and_range_are_not_nonfinite_masks(self):
        q = np.float32(1e-40)
        d, _, _, spec = evaluate(q, 1, 1, 1, 0.04)
        self.assertGreater(d, decimal(np.finfo(np.float32).max))
        self.assertLess(spec, decimal(np.finfo(np.float32).max))
        self.assertEqual(evaluate(q, 1, 1, 1, 0)[-1], 0)
        result = compare([np.inf, np.nan, 0, np.inf], [float(spec), 0, 0, float(d)])
        self.assertEqual(result["nonfinite"], 3)
        self.assertEqual(result["bad_representable"], 2)
        self.assertEqual(result["reference_outside_float32"], 1)

    def test_source_underflow_and_adjacent_cosines_are_explicit(self):
        rows, groups = fixtures()
        self.assertEqual(len(rows), len(groups))
        self.assertIn("cosine_rounding_to_physical_one", groups)
        self.assertTrue(
            np.any(rows[:, 1] == np.nextafter(np.float32(1), np.float32(0)))
        )
        self.assertTrue(
            np.any((rows[:, 0] > 0) & ((rows[:, 0] * rows[:, 0]) ** 2 == 0))
        )
        self.assertEqual(evaluate(0, 1, 1, 1)[2:], (Decimal(0), Decimal(0)))
        with self.assertRaises(ValueError):
            evaluate(0.1, 1, 1, 1.1)

    def test_exact_source_reference_does_not_repeat_float32_underflow(self):
        row = np.array(
            [np.finfo(np.float32).smallest_subnormal, 1, 1, 1, 1, 0.04, 0, 0],
            dtype="<f4",
        )
        exact = row_reference(row)
        captured = row_reference(row, captured_a2=0)
        self.assertGreater(exact[-1], Decimal("1e170"))
        self.assertEqual(captured[-1], 0)


if __name__ == "__main__":
    unittest.main()
