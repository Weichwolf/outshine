"""Independent physical and numerical controls for the source-grid experiment."""

import unittest

import numpy as np
from air_source_grid_reference import (
    DIFFUSE,
    DIRECT,
    SIGMA,
    ShadowControl,
    controls,
    fitted_transfer,
    grid_edges,
    reference,
    relative_errors,
)


class SourceGridTests(unittest.TestCase):
    def test_null_cloud_matches_the_closed_form_and_zero_distance(self):
        query = np.array([0, 1e-8, 0.1, 1.2, 20.0])
        case = ShadowControl(0.45, 0.2, 0, 20)
        exact = -np.expm1(-query[:, None] * SIGMA) / SIGMA * (DIRECT + DIFFUSE)
        np.testing.assert_array_equal(reference(query, case), exact)
        for kind in ("quadratic", "cubic", "quartic", "log", "metric"):
            np.testing.assert_allclose(
                fitted_transfer(query, grid_edges(100, kind), case),
                exact,
                rtol=2e-13,
                atol=1e-16,
            )

    def test_shadow_reference_refines_independently_for_all_declared_controls(self):
        for extent in (np.sqrt(9 - 1.02**2), 100, np.sqrt(6460**2 - 6360.02**2)):
            for case in controls(extent):
                query = np.linspace(0, case.query_end_km, 33)
                low, high = reference(query, case, 64), reference(query, case, 128)
                np.testing.assert_allclose(low, high, rtol=1e-12, atol=1e-13)
                self.assertTrue(np.all(np.isfinite(high) & (high >= 0)))

    def test_physical_extent_exposes_aliasing_without_changing_the_query(self):
        case = ShadowControl(0.45, 0.2, 2, 1.2)
        query = np.linspace(0, 1.2, 121)
        expected = reference(query, case)
        earth = np.sqrt(6460**2 - 6360.02**2)
        coarse = fitted_transfer(query, grid_edges(earth, "quadratic"), case)
        refined = fitted_transfer(query, grid_edges(earth, "log", 256), case)
        self.assertGreater(relative_errors(coarse, expected).max(), 0.5)
        self.assertLess(relative_errors(refined, expected).max(), 0.01)

    def test_query_selection_does_not_change_the_physical_grid_or_transfer(self):
        case = ShadowControl(3, 0.05, 2, 3.2)
        query = np.linspace(0, 3.2, 129)
        edges = grid_edges(1132, "log", 256)
        dense = fitted_transfer(query, edges, case)
        sparse = fitted_transfer(query[::16], edges, case)
        np.testing.assert_array_equal(dense[::16], sparse)


if __name__ == "__main__":
    unittest.main()
