"""Whole grade, fixed provenance, coordinate limitations and real contact shape."""

import unittest
from unittest.mock import Mock, patch

import numpy as np
from ground_contact import ContactLimits
from ground_contact_checks import qualify_contact
from ground_contact_inputs import load_context
from ground_interior import relax_contact, relax_interior
from ground_refinement import vertical_lod_difference
from ground_rim_contact import build_rim_contact
from ground_rim_fit import fit_rim_heights
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import _face_gradients


class CoordinateSweeps(unittest.TestCase):
    def setUp(self):
        self.mesh = Mesh(
            np.array(
                [
                    [-1.0, -1.0, 0.0],
                    [1.0, -1.0, 0.0],
                    [1.0, 1.0, 0.0],
                    [-1.0, 1.0, 0.0],
                    [0.0, 0.0, 0.0],
                ]
            ),
            np.array([[0, 1, 4], [1, 2, 4], [2, 3, 4], [3, 0, 4]]),
        )
        self.options = {"fixed_vertices": 4, "insertion_counts": (1,), "grade": 1.0}

    def test_analytic_active_grade_and_immutable_base(self):
        before = self.mesh.vertices.tobytes()
        result, report = relax_interior(
            self.mesh, lambda q: np.full(len(q), 10.0), **self.options
        )
        self.assertAlmostEqual(result.vertices[-1, 2], 1.0, places=12)
        np.testing.assert_array_equal(result.vertices[:4], self.mesh.vertices[:4])
        np.testing.assert_array_equal(result.vertices[:, :2], self.mesh.vertices[:, :2])
        np.testing.assert_array_equal(result.triangles, self.mesh.triangles)
        self.assertEqual(self.mesh.vertices.tobytes(), before)
        self.assertEqual(report["target_queries"], 1)
        self.assertLessEqual(report["incidence_updates"], 32768)
        self.assertTrue(report["accepted"])

    def test_zero_or_insufficient_work_reuses_valid_base_without_observations(self):
        for limits in (
            {"maximum_sweeps": 0},
            {"maximum_incidence_updates": 0},
            {"maximum_incidence_updates": 3},
        ):
            target = Mock(side_effect=AssertionError("queried without capacity"))
            result, report = relax_interior(self.mesh, target, **self.options, **limits)
            self.assertIs(result, self.mesh)
            self.assertEqual(report["incidence_updates"], 0)
            self.assertEqual(report["target_queries"], 0)

    def test_missing_targets_are_retained_and_identified(self):
        result, report = relax_interior(
            self.mesh, lambda q: np.array([np.nan]), **self.options
        )
        np.testing.assert_array_equal(result.vertices, self.mesh.vertices)
        self.assertEqual(report["missing_vertex_ids"], [4])
        self.assertEqual(report["reason"], "no_coordinate_move")

    def test_invalid_partition_targets_caps_and_protected_faces(self):
        for options in (
            {"insertion_counts": ()},
            {"fixed_vertices": -1},
            {"maximum_sweeps": True},
            {"maximum_incidence_updates": -1},
            {"grade": np.inf},
            {"protected_faces": 1},
        ):
            with self.assertRaises(ValueError):
                relax_interior(
                    self.mesh, lambda q: np.zeros(len(q)), **(self.options | options)
                )
        with self.assertRaises(ValueError):
            relax_interior(self.mesh, lambda q: np.zeros((len(q), 1)), **self.options)
        invalid = Mesh(
            self.mesh.vertices
            + np.c_[np.zeros((5, 2)), np.array([0.0, 0.0, 0.0, 0.0, 2.0])],
            self.mesh.triangles,
        )
        with self.assertRaisesRegex(ValueError, "grade"):
            relax_interior(invalid, lambda q: np.zeros(len(q)), **self.options)

    def test_face_order_does_not_change_the_physical_result(self):
        reverse = Mesh(self.mesh.vertices, self.mesh.triangles[::-1])
        a, _ = relax_interior(self.mesh, lambda q: np.array([10.0]), **self.options)
        b, _ = relax_interior(reverse, lambda q: np.array([10.0]), **self.options)
        np.testing.assert_array_equal(a.vertices, b.vertices)

    def test_failed_final_gradient_check_retains_the_valid_base(self):
        def invalid_step(vertices, *_args):
            vertices[-1, 2] = 100.0
            return 100.0

        with patch("ground_interior._sweep", side_effect=invalid_step):
            result, report = relax_interior(
                self.mesh, lambda q: np.array([10.0]), **self.options
            )
        self.assertIs(result, self.mesh)
        self.assertFalse(report["accepted"])
        self.assertEqual(report["reason"], "whole_gradient_postcheck_failed")

    def test_separate_coordinates_can_stall_where_a_coupled_move_improves(self):
        mesh = Mesh(
            np.array([[0.0, 0.0, 0.0], [1.0, 0.0, 1.0], [0.0, 1.0, 0.0]]),
            np.array([[0, 1, 2]]),
        )
        target = lambda q: np.array([2.0, 1.0])
        result, report = relax_interior(
            mesh, target, fixed_vertices=1, insertion_counts=(1, 1), grade=1.0
        )
        np.testing.assert_array_equal(result.vertices, mesh.vertices)
        self.assertEqual(report["reason"], "no_coordinate_move")
        prior = mesh.vertices.copy()
        prior[1:, 2] = target(None)
        coupled = fit_rim_heights(
            Mesh(prior, mesh.triangles), np.eye(2)[None], {0: 0.0}, maximum_change_m=8.0
        )
        self.assertIsNotNone(coupled.mesh)
        np.testing.assert_allclose(
            coupled.mesh.vertices[1:, 2],
            np.array([2.0, 1.0]) / np.sqrt(5.0),
            atol=1e-5,
            rtol=0,
        )
        self.assertLess(
            np.linalg.norm(coupled.mesh.vertices[1:, 2] - target(None)), np.sqrt(2.0)
        )
        with self.assertRaisesRegex(ValueError, "adjacent"):
            relax_interior(
                mesh, target, fixed_vertices=1, insertion_counts=(2,), grade=1.0
            )


class ActualInterior(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.context = load_context()
        c = cls.context
        cls.base = build_rim_contact(
            c.road, c.road_dem_z, c.sample_z, limits=ContactLimits(margins_m=(15.0,))
        )
        cls.result = relax_contact(c.road, cls.base, c.sample_z)

    def test_complete_real_contact_and_all_coarse_values_are_preserved(self):
        q = qualify_contact(self.context.road, self.result)
        self.assertEqual(
            (q["vertices"], q["faces"], q["whole_road_soil_edges"]), (315, 564, 90)
        )
        n = len(self.base["coarse_mesh"].vertices)
        np.testing.assert_array_equal(
            self.result["mesh"].vertices[:n], self.base["coarse_mesh"].vertices
        )
        np.testing.assert_array_equal(
            self.result["mesh"].triangles, self.base["mesh"].triangles
        )
        self.assertIs(self.result["rim_profile"], self.base["rim_profile"])
        self.assertEqual(self.result["interior_fit"]["target_queries"], 128)
        self.assertEqual(self.result["interior_fit"]["sweeps"], 2)

    def test_unfitted_soil_centroids_improve_at_the_same_topology(self):
        base, result = self.base["mesh"], self.result["mesh"]
        faces = base.triangles[len(self.context.road.triangles) :]
        triangles = base.vertices[faces]
        areas = (
            np.cross(
                triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
            )[:, 2]
            / 2
        )
        truth = self.context.sample_z(triangles[:, :, :2].mean(axis=1))
        errors = [
            mesh.vertices[faces, 2].mean(axis=1) - truth for mesh in (base, result)
        ]
        self.assertLess(
            np.average(errors[1] ** 2, weights=areas),
            np.average(errors[0] ** 2, weights=areas),
        )
        self.assertLess(abs(errors[1]).max(), abs(errors[0]).max())
        n = len(self.base["coarse_mesh"].vertices)
        target = self.context.sample_z(base.vertices[n:, :2])
        self.assertTrue(
            np.all(
                abs(result.vertices[n:, 2] - target)
                <= abs(base.vertices[n:, 2] - target) + 1e-10
            )
        )

    def test_dense_unfitted_controls_improve_but_are_not_a_continuous_bound(self):
        from ground_interior_validation import validate, validation_inputs

        inputs = validation_inputs(self.context, self.base)
        before, after = [
            validate(value["mesh"], inputs) for value in (self.base, self.result)
        ]
        self.assertEqual(inputs["dense_dem_z"].size, 410 * 45)
        for key in ("dense_max_abs_error_m", "dense_area_weighted_rms_m"):
            self.assertLess(after[key], before[key])

    def test_partial_missing_targets_and_one_sweep_work_cap(self):
        n = len(self.base["coarse_mesh"].vertices)

        def sample(xy):
            heights = self.context.sample_z(xy)
            heights[0] = np.nan
            return heights

        result = relax_contact(
            self.context.road, self.base, sample, maximum_incidence_updates=612
        )
        self.assertEqual(result["interior_fit"]["sweeps"], 1)
        self.assertEqual(result["interior_fit"]["incidence_updates"], 612)
        self.assertEqual(result["interior_fit"]["missing_vertex_ids"], [n])
        np.testing.assert_array_equal(
            result["mesh"].vertices[n], self.base["mesh"].vertices[n]
        )
        qualify_contact(self.context.road, result)

    def test_bridge_walking_surfaces_remain_above_the_whole_soil_overlap(self):
        from ground_interior_validation import bridge_composition

        report, arrays = bridge_composition()
        self.assertEqual(len(report["bridges"]), 2)
        self.assertIn("bridge_soil_faces", arrays)
        for bridge in report["bridges"]:
            self.assertGreater(bridge["soil_to_top"]["positive_area_pairs"], 0)
            self.assertGreaterEqual(bridge["soil_to_top"]["minimum_clearance_m"], 0.0)

    def test_geometric_lod_bound_is_recomputed_and_bounds_child_interiors(self):
        fine, coarse, roots = (
            self.result["mesh"],
            self.result["coarse_mesh"],
            self.result["refinement_roots"],
        )
        bound = self.result["refinement"]["maximum_vertical_LOD_difference_m"]
        self.assertEqual(bound, vertical_lod_difference(coarse, fine, roots))
        stale = self.base | {
            "refinement": self.base["refinement"]
            | {"maximum_vertical_LOD_difference_m": -1.0}
        }
        renewed = relax_contact(
            self.context.road, stale, self.context.sample_z, maximum_sweeps=0
        )
        self.assertEqual(
            renewed["refinement"]["maximum_vertical_LOD_difference_m"],
            vertical_lod_difference(coarse, self.base["mesh"], roots),
        )
        gradient = _face_gradients(
            coarse.vertices[:, :2], coarse.triangles, coarse.vertices[:, 2]
        )
        p0 = coarse.vertices[coarse.triangles[roots, 0]]
        for weights in ([1.0, 0.0, 0.0], [0.2, 0.3, 0.5], [1 / 3, 1 / 3, 1 / 3]):
            p = np.einsum("j,fji->fi", weights, fine.vertices[fine.triangles])
            z = p0[:, 2] + np.einsum("fi,fi->f", p[:, :2] - p0[:, :2], gradient[roots])
            self.assertLessEqual(float(abs(p[:, 2] - z).max()), bound + 1e-10)


if __name__ == "__main__":
    unittest.main()
