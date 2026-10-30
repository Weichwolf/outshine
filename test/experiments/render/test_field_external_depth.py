"""Actual finite-point ownership, conservative unknowns and parent preservation."""

import unittest
from dataclasses import replace
from fractions import Fraction

import numpy as np
from field_external_depth import (
    PointSamples,
    SampleBinding,
    compose_registered,
    interval,
)
from field_view_integrals import box_integrals, summed_field


def samples(z, colour, *, source="whole-building-v1"):
    z = np.asarray(z, np.float32)
    rgba = np.zeros((*z.shape, 4), np.float32)
    rgba[z > 0, :3] = colour
    rgba[z > 0, 3] = 1
    binding = SampleBinding(
        "frame", "exact-rays", "world", (source,), "mat", "sun", "eye"
    )
    return PointSamples(binding, rgba, z, np.zeros(z.shape), np.ones(z.shape, bool))


def compose(a, b, **kwargs):
    return compose_registered(a, b, wanted_a=a.binding, wanted_b=b.binding, **kwargs)


class ExternalDepthTests(unittest.TestCase):
    def test_no_common_hits_and_fully_covered_rasters_preserve_exact_ownership(self):
        cases = (
            ([[0, 0], [0, 0]], [[0, 0], [0, 0]]),
            ([[0.8, 0], [0, 0.2]], [[0, 0.7], [0.4, 0]]),
            ([[0.8, 0.2], [0.6, 0.1]], [[0.3, 0.7], [0.4, 0.9]]),
        )
        for dtype in (np.float32, np.float64):
            for left, right in cases:
                a, b = samples(left, [3, 2, 1]), samples(right, [1, 4, 2])
                a, b = [
                    replace(
                        s,
                        rgba=s.rgba.astype(dtype),
                        reverse_z=s.reverse_z.astype(dtype),
                    )
                    for s in (a, b)
                ]
                result = compose(a, b)
                nearer = a.reverse_z > b.reverse_z
                np.testing.assert_array_equal(
                    result.rgba, np.where(nearer[:, :, None], a.rgba, b.rgba)
                )
                np.testing.assert_array_equal(
                    result.reverse_z, np.maximum(a.reverse_z, b.reverse_z)
                )
                np.testing.assert_array_equal(result.error, np.zeros((2, 2)))

    def test_outward_intervals_enclose_exact_binary_operands(self):
        rng = np.random.default_rng(1797)
        a = samples(rng.uniform(0.01, 0.99, (3, 7)), [2, 1, 0])
        a = replace(a, error=rng.uniform(0, 0.01, a.error.shape))
        lo, hi = interval(a)
        for z, e, low, high in zip(
            a.reverse_z.ravel(), a.error.ravel(), lo.ravel(), hi.ravel(), strict=True
        ):
            exact_z, exact_e = Fraction(float(z)), Fraction(float(e))
            self.assertLessEqual(Fraction(low), max(0, exact_z - exact_e))
            self.assertGreaterEqual(Fraction(high), min(1, exact_z + exact_e))

    def test_interleave_and_clear_holes_match_independent_scalar_nearest(self):
        a = samples([[0.8, 0.2, 0, 0], [0, 0.9, 0.1, 0.3]], [3, 2, 1])
        b = samples(
            [[0.4, 0.7, 0.5, 0], [0.5, 0, 0.8, 0.1]], [0.2, 0.4, 0.6], source="terrain"
        )
        old = [
            x.copy() for s in (a, b) for x in (s.rgba, s.reverse_z, s.error, s.known)
        ]
        result = compose(a, b)
        self.assertEqual(result.bindings, (a.binding, b.binding))
        expected = np.empty_like(a.rgba)
        for y, x in np.ndindex(a.reverse_z.shape):
            owner = a if a.reverse_z[y, x] > b.reverse_z[y, x] else b
            expected[y, x] = owner.rgba[y, x]
        np.testing.assert_array_equal(result.rgba, expected)
        for actual, before in zip(
            (x for s in (a, b) for x in (s.rgba, s.reverse_z, s.error, s.known)),
            old,
            strict=True,
        ):
            np.testing.assert_array_equal(actual, before)
            self.assertFalse(np.shares_memory(result.rgba, actual))

    def test_composition_precedes_filtering_preserves_energy_and_coverage(self):
        a = samples([[0.8, 0]], [2, 0, 0])
        b = samples([[0.4, 0]], [0, 1, 0], source="terrain")
        result = compose(a, b)
        field = summed_field(result.rgba, dtype=np.float64)
        value = box_integrals(field, np.array([[0.0, 0]]), np.array([[2.0, 1]]))[0] / 2
        np.testing.assert_array_equal(value, [1, 0, 0, 0.5])
        af, bf = a.rgba.mean(1), b.rgba.mean(1)
        wrong = af + (1 - af[:, 3:]) * bf
        self.assertEqual(float(wrong[0, 3]), 0.75)

    def test_unknown_is_never_transparent_and_ties_do_not_replace_parent(self):
        a, b = samples([[0.5]], [1, 0, 0]), samples([[0.5]], [0, 1, 0], source="ground")
        for s in (
            b,
            replace(
                b, reverse_z=np.array([[0.50001]], np.float32), error=np.array([[0.01]])
            ),
            replace(b, known=np.zeros((1, 1), bool)),
        ):
            result = compose(a, s)
            self.assertIsNone(result.rgba)
        clear = samples([[0]], [0, 0, 0])
        self.assertIsNone(compose(replace(clear, known=np.zeros((1, 1), bool)), b).rgba)

    def test_every_binding_change_invalidates_held_product(self):
        a, b = samples([[0.8]], [1, 0, 0]), samples([[0.2]], [0, 1, 0])
        self.assertIsNone(compose(replace(a, binding=None), b).rgba)
        self.assertIsNone(
            compose_registered(a, b, wanted_a=None, wanted_b=b.binding).rgba
        )
        for field in a.binding.__dataclass_fields__:
            value = ("other",) if field == "sources" else "other"
            changed = replace(a, binding=replace(a.binding, **{field: value}))
            self.assertIsNone(
                compose_registered(
                    changed, b, wanted_a=a.binding, wanted_b=b.binding
                ).rgba
            )
        for field in ("frame", "capture", "world_revision", "lighting", "pbr_view"):
            self.assertIsNone(
                compose(
                    a, replace(b, binding=replace(b.binding, **{field: "other"}))
                ).rgba
            )

    def test_invalid_capacity_fractional_colour_depth_and_error_rejected(self):
        a, b = samples([[0.8, 0.7]], [1, 0, 0]), samples([[0.2, 0.3]], [0, 1, 0])
        self.assertIsNone(compose(a, b, texels_most=1).rgba)
        with self.assertRaises(ValueError):
            compose(a, b, texels_most=0)
        for field, value in (
            ("rgba", np.full((1, 2, 4), 0.5, np.float32)),
            ("reverse_z", np.array([[np.nan, 0.5]], np.float32)),
            ("error", np.array([[-0.1, 0]])),
            ("known", np.ones((1, 2))),
        ):
            with self.subTest(field=field):
                result = compose(replace(a, **{field: value}), b)
                self.assertEqual(result.reason, "invalid_or_over_capacity_samples")
                self.assertIsNone(result.rgba)


if __name__ == "__main__":
    unittest.main()
