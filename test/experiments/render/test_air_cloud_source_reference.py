"""Physical source/transport counterexamples for #320, without runtime claims."""

import unittest
from dataclasses import replace

import numpy as np
from air_cloud_source_reference import MODES, Slab, analytic, log_weight, quadrature


class SourceShadowTests(unittest.TestCase):
    def test_closed_form_matches_independent_refined_quadrature(self):
        for height in (0.2, 1.5, 3.0):
            for view_mu in (-0.6, 0.0, 0.4, 0.8):
                for sun_mu in (-0.2, 0.2, 0.8):
                    slab = Slab(eye_height=height, view_mu=view_mu, sun_mu=sun_mu)
                    for query in (0.0, 0.1, 2.0, 6.0, 12.0):
                        expected = analytic(slab, query)
                        np.testing.assert_allclose(
                            expected,
                            quadrature(slab, query, 32),
                            atol=1e-13,
                            rtol=1e-12,
                        )
                        np.testing.assert_allclose(
                            expected,
                            quadrature(slab, query, 64),
                            atol=1e-13,
                            rtol=1e-12,
                        )
                        self.assertTrue(np.all(np.isfinite(expected) & (expected >= 0)))

    def test_no_cloud_keeps_all_source_alternatives_identical(self):
        slab = Slab(cloud_sigma=0)
        for query in (0.0, 0.1, 2.0, 6.0, 12.0):
            expected = analytic(slab, query)
            for mode in MODES:
                np.testing.assert_array_equal(expected, analytic(slab, query, mode))

    def test_prescribed_diffuse_input_is_preserved_when_the_sun_is_down(self):
        slab = Slab(sun_mu=-0.2)
        for query in (0.1, 2.0, 6.0):
            for mode in MODES:
                np.testing.assert_array_equal(
                    analytic(slab, query), analytic(slab, query, mode)
                )

    def test_wrong_source_methods_fail_the_unchanged_one_percent_control(self):
        slab = Slab()
        expected = analytic(slab, 12)
        for mode in MODES[1:]:
            error = np.max(
                np.abs(analytic(slab, 12, mode) - expected) / np.maximum(1e-5, expected)
            )
            self.assertGreater(error, 0.01, mode)

    def test_segments_compose_without_a_second_source_or_sun_factor(self):
        for slab in (
            Slab(),
            Slab(eye_height=3, view_mu=-0.3),
            Slab(view_mu=0.8, sun_mu=0.8),
        ):
            for first, second in ((0.1, 2.0), (2.0, 6.0), (6.0, 12.0)):
                split = slab.reach(first)
                end = slab.reach(second)
                after = replace(slab, eye_height=float(slab.height(split)))
                transfer = np.exp(log_weight(slab, split, direct=False, shadow=False))
                composed = analytic(slab, split) + transfer * analytic(
                    after, end - split
                )
                np.testing.assert_allclose(
                    composed, analytic(slab, end), atol=1e-14, rtol=1e-12
                )


if __name__ == "__main__":
    unittest.main()
