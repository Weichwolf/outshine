"""Conforming geometry, preserved source seams, actual interpolation and bounded work."""

import unittest

import numpy as np
from road_normal_lod import NormalBudget, prepare, sample
from road_surface_lod import prepare as position_only
from road_surface_model import Curve
from test_road_surface_lod import recipe


class TensorNormals(unittest.TestCase):
    def test_flat_source_is_whole_without_unnecessary_refinement(self):
        p = prepare(Curve(recipe()), 0.25, 0.1)
        self.assertEqual(p["positions_enh_m"].shape, (4, 3))
        self.assertEqual(len(p["indices"]), 2)
        np.testing.assert_array_equal(p["normals_enh"], np.tile([0, 0, 1], (4, 1)))

    def test_curved_grade_refines_longitudinally_and_matches_closed_normal(self):
        p = prepare(Curve(recipe(height=0.01)), 0.25, 0.1)
        self.assertGreater(len(p["stations_m"]), 2)
        self.assertEqual(len(p["lateral_m"]), 2)
        s = np.linspace(0, 10, 41)
        u = np.array([-2.0, -0.1, 2.0])
        got = sample(p, s, u)
        exact = np.c_[-0.02 * s, np.zeros(len(s)), np.ones(len(s))]
        exact /= np.linalg.norm(exact, axis=1)[:, None]
        angles = np.arctan2(
            np.linalg.norm(np.cross(got, exact[:, None]), axis=-1),
            np.sum(got * exact[:, None], axis=-1),
        )
        self.assertLessEqual(angles.max(), 0.1)
        self.assertLessEqual(p["maximum_stored_position_bound_m"], 0.25)

    def test_uniform_comparison_can_refine_only_the_needed_parameter_axis(self):
        p = prepare(Curve(recipe(height=0.01)), 0.25, 0.1, uniform=True)
        self.assertGreater(len(p["stations_m"]), 2)
        self.assertEqual(len(p["lateral_m"]), 2)

    def test_source_seam_positions_match_exactly_with_independent_vertex_rows(self):
        p = prepare(Curve(recipe(height=0.01, knots=3)), 0.25, 0.1)
        at = np.flatnonzero(p["station_lateral_m"][:, 0] == 5)
        self.assertEqual(len(at), 2 * len(p["lateral_m"]))
        a, b = np.split(at, 2)
        np.testing.assert_array_equal(p["positions_enh_m"][a], p["positions_enh_m"][b])
        self.assertFalse(np.any(np.isin(a, b)))

    def test_limits_reject_locally_and_never_mutate_the_default_path(self):
        c = Curve(recipe(height=0.01))
        old = position_only(c, 0.25)
        for options in ({"max_queries": 1}, {"max_vertices": 4}):
            with self.assertRaises(NormalBudget):
                prepare(c, 0.25, 0.01, **options)
        for key in ("positions_enh_m", "indices", "stations_m"):
            np.testing.assert_array_equal(old[key], position_only(c, 0.25)[key])
        with self.assertRaises(ValueError):
            prepare(c, 0.25, 0)
        with self.assertRaises(ValueError):
            sample(prepare(Curve(recipe()), .25, .1), [-1], [0])


if __name__ == "__main__":
    unittest.main()
