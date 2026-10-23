"""Perspective error envelopes and observational reconstruction controls."""

import unittest

import numpy as np
from field_reconstruction_audit import (
    POSITIVE_CORNERS,
    PROBE_SIGNS,
    VALUE_FIT,
    ReconstructionAudit,
)
from field_scale_features import feature_summary, source_features
from field_scale_projection import displacement_bound, position_budget, project
from reconstructed_source_field import UNKNOWN, FieldBuilder
from test_source_support_function import compile_surfaces, shape, surface


def view():
    return np.array([0, 0, -1000.0]), np.eye(3), 1000.0, 1280, 720


class ProjectionBoundTest(unittest.TestCase):
    def test_lateral_position_and_depth_displacement_are_different(self):
        points = np.array([[0, 0, 0], [1, 0, 0], [0, 0, 1]])
        image = project(points, view())
        np.testing.assert_allclose(image, [[640, 360], [641, 360], [640, 360]])

    def test_half_pixel_budget_bounds_finite_perspective_displacements(self):
        bounds = np.array([[300, -170, -20], [700, 400, 200]])
        radius = position_budget(bounds, view())
        self.assertAlmostEqual(displacement_bound(bounds, view(), radius), 0.5)
        rng = np.random.default_rng(84)
        points = rng.uniform(bounds[0], bounds[1], (10000, 3))
        delta = rng.normal(size=points.shape)
        delta *= radius / np.linalg.norm(delta, axis=1)[:, None]
        actual = np.linalg.norm(
            project(points + delta, view()) - project(points, view()), axis=1
        )
        self.assertLessEqual(float(actual.max()), 0.5)

    def test_off_axis_slope_and_near_depth_are_not_omitted(self):
        on_axis = np.array([[0, 0, 0], [0, 0, 0]])
        off_axis = np.array([[2000, 0, 0], [2000, 0, 0]])
        self.assertGreater(
            displacement_bound(off_axis, view(), 1),
            displacement_bound(on_axis, view(), 1),
        )
        centre = np.array([[2000, 0, 0]])
        moved = centre + [0, 0, -1]
        exact = float(np.linalg.norm(project(moved, view()) - project(centre, view())))
        self.assertLessEqual(exact, displacement_bound(off_axis, view(), 1))
        self.assertGreater(exact, 1)

    def test_bound_rejects_near_crossing_and_nonorthonormal_view(self):
        with self.assertRaises(ValueError):
            displacement_bound(np.zeros((2, 3)), view(), 1000)
        bad = list(view())
        bad[1] = np.diag([1, 1, 2])
        with self.assertRaises(ValueError):
            project(np.zeros((1, 3)), bad)

    def test_geometric_position_budget_is_not_a_coverage_bound(self):
        original = np.array([[-0.1, 0, 0], [0.1, 0, 0]])
        joined = np.array([[0, 0, 0], [0, 0, 0]])
        shift = np.linalg.norm(
            project(joined, view()) - project(original, view()), axis=1
        )
        self.assertTrue((shift < 0.5).all())
        self.assertGreater(
            np.linalg.norm(np.diff(project(original, view()), axis=0)), 0
        )
        self.assertEqual(np.linalg.norm(np.diff(project(joined, view()), axis=0)), 0)


class ReconstructionAuditTest(unittest.TestCase):
    def test_observer_preserves_every_field_array_and_accounts_for_needs(self):
        a = shape(ring=[[0, 0], [1.23, 0], [1.23, 4], [0, 4]], eaves_m=4)
        b = shape(ring=[[1.25, 0], [4, 0], [4, 4], [1.25, 4]], eaves_m=4)
        support = compile_surfaces([surface([a]), surface([b])])[0]
        original, _ = FieldBuilder(support, max_nodes=9).run()
        audit = ReconstructionAudit()
        observed, _ = FieldBuilder(support, max_nodes=9, observer=audit).run()
        for key in original:
            np.testing.assert_array_equal(original[key], observed[key])
        summary = audit.summary()
        self.assertEqual(
            summary["unknown_nodes"], int(((original["tags"] & UNKNOWN) != 0).sum())
        )
        self.assertGreater(summary["node_budget_stops"], 0)
        self.assertEqual(summary["minimum_side_stops"], 0)

    def test_all_positive_corners_are_not_blown_up_to_full_coverage(self):
        small = shape(
            ring=[[0.01, 0.01], [0.02, 0.01], [0.02, 0.02], [0.01, 0.02]],
            sole_m=0.25,
            eaves_m=0.75,
        )
        support = compile_surfaces([surface([small])])[0]
        audit = ReconstructionAudit()
        field, _ = FieldBuilder(support, max_nodes=1, observer=audit).run()
        self.assertTrue(int(field["tags"][0]) & UNKNOWN)
        masks = audit.arrays()["decisions"][:, 4].astype(int)
        self.assertTrue((masks & POSITIVE_CORNERS).any())
        self.assertTrue((masks & PROBE_SIGNS).any())
        self.assertTrue((masks & VALUE_FIT).any())

    def test_real_minimum_depth_stop_is_reported_separately_from_budget(self):
        small = shape(
            ring=[[0.01, 0.01], [0.02, 0.01], [0.02, 0.02], [0.01, 0.02]],
            sole_m=0.25,
            eaves_m=0.75,
        )
        support = compile_surfaces([surface([small])])[0]
        audit = ReconstructionAudit()
        FieldBuilder(support, minimum_side=1, observer=audit).run()
        self.assertEqual(audit.summary()["node_budget_stops"], 0)
        self.assertEqual(audit.summary()["minimum_side_stops"], 1)

    def test_actual_two_centimetre_footprint_connector_is_measured_in_view(self):
        support, basis, anchor, _ = compile_surfaces(
            [surface([shape()]), surface([shape()], origin=(2.02, 0, 0))]
        )
        v = view()
        transformed = v[0] @ basis.T - anchor, v[1] @ basis.T, *v[2:]
        features, counts = source_features(support, transformed)
        self.assertEqual(counts["nearest_unique_pairs"], 1)
        np.testing.assert_allclose(features["gap_width_m"], [0.02], atol=1e-12)
        self.assertAlmostEqual(features["gap_width_px"][0], 1000 * 0.02 / 1001)
        self.assertEqual(feature_summary(features)["nearest_gap_below_half_pixel"], 1)
