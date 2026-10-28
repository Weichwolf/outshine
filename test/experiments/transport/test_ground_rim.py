"""Bounded profile observation, equivalent quadratic fits and whole real outputs."""

import unittest
from unittest.mock import patch

import numpy as np
from ground_contact import ContactLimits, build_contact
from ground_contact_checks import qualify_contact
from ground_contact_inputs import load_context
from ground_rim_contact import build_rim_contact
from ground_rim_fit import fit_rim_heights
from ground_rim_sampling import RimRecipe, observe_rim
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import HeightBudget


def fit_input():
    mesh = Mesh(
        np.array(
            [
                [0.0, 0.0, 0.0],
                [1.0, 0.0, 0.0],
                [1.0, 1.0, 2.0],
                [0.0, 1.0, 0.0],
                [-1.0, 0.0, 0.0],
            ]
        ),
        np.array([[0, 1, 2], [0, 2, 3], [4, 0, 3]], dtype=np.uint32),
    )
    return mesh, np.broadcast_to(np.eye(2), (3, 2, 2)), {i: 0.0 for i in (0, 1, 3, 4)}


class QuadraticFits(unittest.TestCase):
    def test_full_and_eliminated_problem_match_known_unique_solution(self):
        mesh, frames, fixed = fit_input()
        values = [
            fit_rim_heights(mesh, frames, fixed, eliminate_fixed=method)
            for method in (False, True)
        ]
        for value in values:
            self.assertIsNotNone(value.mesh)
            self.assertAlmostEqual(value.mesh.vertices[2, 2], 1.0, places=7)
            self.assertEqual(
                value.mesh.vertices[list(fixed)].tobytes(),
                mesh.vertices[list(fixed)].tobytes(),
            )
            self.assertEqual(value.mesh.triangles.tobytes(), mesh.triangles.tobytes())
        np.testing.assert_allclose(
            values[0].mesh.vertices, values[1].mesh.vertices, atol=1e-7, rtol=0
        )
        self.assertEqual(values[1].report["solver_variables"], 1)
        self.assertEqual(values[1].report["omitted_fixed_faces"], 1)
        self.assertLess(
            values[1].report["matrix_nonzeros"], values[0].report["matrix_nonzeros"]
        )

    def test_already_valid_prior_is_reused_without_solver_work(self):
        mesh, frames, fixed = fit_input()
        vertices = mesh.vertices.copy()
        vertices[2, 2] = 0.5
        source = Mesh(vertices, mesh.triangles)
        with patch("ground_rim_fit.solve_rim", side_effect=AssertionError("solved")):
            result = fit_rim_heights(source, frames, fixed)
        self.assertIs(result.mesh, source)

    def test_fixed_face_conflict_and_impossible_observation_bound_stay_pending(self):
        mesh, frames, fixed = fit_input()
        self.assertIsNone(
            fit_rim_heights(mesh, frames, fixed, maximum_change_m=0.1).mesh
        )
        vertices = mesh.vertices.copy()
        vertices[4, 2] = 4
        self.assertIsNone(
            fit_rim_heights(
                Mesh(vertices, mesh.triangles), frames, fixed | {4: 4.0}
            ).mesh
        )

    def test_missing_caps_or_time_prevent_solver_work(self):
        mesh, frames, fixed = fit_input()
        for budget in (
            HeightBudget(time_limit_s=0),
            HeightBudget(max_vertices=4),
            HeightBudget(max_faces=2),
        ):
            with patch(
                "ground_rim_fit.solve_rim", side_effect=AssertionError("solved")
            ):
                self.assertIsNone(
                    fit_rim_heights(mesh, frames, fixed, budget=budget).mesh
                )

    def test_invalid_pins_weights_and_frames_are_rejected(self):
        mesh, frames, fixed = fit_input()
        for options in (
            {"fixed": {0: 1.0}},
            {"fixed": {9: 0.0}},
            {"fixed": {True: 0.0}},
            {"weights": np.zeros(5)},
            {"maximum_change_m": np.nan},
            {"frames": np.zeros((3, 2, 2))},
        ):
            kwargs = {"frames": frames, "fixed": fixed} | options
            with self.assertRaises(ValueError):
                fit_rim_heights(mesh, **kwargs)

    def test_translation_does_not_change_the_fitted_relative_height(self):
        mesh, frames, fixed = fit_input()
        shift = np.array([1024.0, -512.0, 128.0])
        changed = Mesh(mesh.vertices + shift, mesh.triangles)
        result = fit_rim_heights(
            changed, frames, {i: v + 128 for i, v in fixed.items()}
        )
        self.assertAlmostEqual(result.mesh.vertices[2, 2] - 128, 1.0, places=7)


class RimObservations(unittest.TestCase):
    corners = np.array([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]])

    def test_affine_field_needs_only_four_adaptive_vertices(self):
        sample = lambda xy: xy[:, 0] + 0.25 * xy[:, 1]
        xy, z, work = observe_rim(
            self.corners, sample(self.corners), sample, RimRecipe(sampling="adaptive")
        )
        np.testing.assert_array_equal(xy, self.corners)
        np.testing.assert_array_equal(z, sample(self.corners))
        self.assertEqual(work["vertices"], 4)

    def test_adaptive_budget_end_does_not_claim_a_met_midpoint_target(self):
        sample = lambda xy: xy[:, 0] ** 2 + xy[:, 1] ** 2
        _, _, work = observe_rim(
            self.corners,
            sample(self.corners),
            sample,
            RimRecipe(sampling="adaptive", maximum_dem_points=8, midpoint_error_m=0.01),
        )
        self.assertEqual(work["reason"], "sampling_budget_exhausted")
        self.assertEqual(work["dem_points"], 8)
        self.assertGreater(work["maximum_tested_midpoint_error_m"], 0.01)

    def test_midpoint_heuristic_does_not_certify_a_hidden_narrow_peak(self):
        def sample(xy):
            return np.where(
                (abs(xy[:, 0] - 0.25) < 0.01) & (abs(xy[:, 1]) < 0.01), 3.0, 0.0
            )

        xy, z, work = observe_rim(
            self.corners, sample(self.corners), sample, RimRecipe(sampling="adaptive")
        )
        self.assertEqual(len(xy), 4)
        self.assertEqual(z.max(), 0.0)
        self.assertEqual(sample(np.array([[0.25, 0.0]]))[0], 3.0)
        self.assertIn("not a continuous", work["scope"])

    def test_invalid_sampling_parameters_and_missing_observations(self):
        for options in (
            {"sampling": "unknown"},
            {"segments_per_side": True},
            {"maximum_vertices": 3},
            {"maximum_dem_points": 0},
            {"maximum_adjustment_m": np.inf},
            {"midpoint_error_m": 0},
        ):
            with self.assertRaises(ValueError):
                RimRecipe(**options)
        with self.assertRaisesRegex(ValueError, "rim_dem"):
            observe_rim(
                self.corners,
                np.zeros(4),
                lambda xy: np.full(len(xy), np.nan),
                RimRecipe(),
            )


class ActualFittedRim(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.context = load_context()
        cls.limits = ContactLimits(margins_m=(15.0,))
        cls.outputs = {
            method: cls.build_result(method=method) for method in ("full", "reduced")
        }

    @classmethod
    def build_result(cls, method="reduced", **options):
        c = cls.context
        return build_rim_contact(
            c.road,
            c.road_dem_z,
            c.sample_z,
            limits=options.pop("limits", cls.limits),
            fit_method=method,
            **options,
        )

    def test_real_source_road_corners_and_whole_profiles_are_preserved(self):
        for result in self.outputs.values():
            q = qualify_contact(self.context.road, result)
            self.assertEqual(
                (
                    q["road_vertices_exact"],
                    q["road_faces_exact"],
                    q["whole_road_soil_edges"],
                ),
                (123, 154, 90),
            )
            self.assertEqual(q["outer_edges"], 64)
            self.assertEqual(len(self.context.report["terminal_values"]), 3)
            self.assertLessEqual(
                result["rim_profile"]["maximum_observation_adjustment_m"], 2.0
            )
            self.assertLessEqual(q["maximum_soil_grade"], (2 / 3) * 1.0000001)

    def test_reduced_and_full_fits_have_same_topology_and_numerical_surface(self):
        a, b = (self.outputs[k]["mesh"] for k in ("full", "reduced"))
        np.testing.assert_array_equal(a.triangles, b.triangles)
        np.testing.assert_array_equal(a.vertices[:, :2], b.vertices[:, :2])
        np.testing.assert_allclose(
            a.vertices[:, 2], b.vertices[:, 2], rtol=0, atol=1e-5
        )
        self.assertEqual(
            self.outputs["reduced"]["rim_profile"]["fit"]["solver_variables"], 60
        )

    def test_adaptive_profile_uses_fewer_vertices_but_is_still_fully_checked(self):
        result = self.build_result(recipe=RimRecipe(sampling="adaptive"))
        q = qualify_contact(self.context.road, result)
        self.assertLess(q["outer_edges"], 64)
        self.assertEqual(q["whole_road_soil_edges"], 90)
        self.assertLessEqual(result["rim_profile"]["sampling"]["dem_points"], 128)

    def test_hard_corner_and_input_budgets_stop_before_profile_construction(self):
        calls = []

        def sample(xy):
            calls.append(len(xy))
            return self.context.sample_z(xy)

        c = self.context
        result = build_rim_contact(
            c.road, c.road_dem_z, sample, limits=ContactLimits(margins_m=(5.0,))
        )
        self.assertEqual(result["status"], "pending")
        self.assertEqual(
            result["candidates"][0]["reason"], "fixed_corner_lipschitz_conflict"
        )
        self.assertEqual(calls, [4])
        for budget in (HeightBudget(time_limit_s=0), HeightBudget(max_vertices=10)):
            result = build_rim_contact(
                c.road, c.road_dem_z, lambda _: self.fail("queried"), budget=budget
            )
            self.assertEqual(result["status"], "pending")

    def test_missing_dem_preserves_source_and_has_no_mesh(self):
        c = self.context
        before = c.road.vertices.tobytes()
        result = build_rim_contact(
            c.road, c.road_dem_z, lambda xy: np.full(len(xy), np.nan)
        )
        self.assertEqual(result["status"], "pending")
        self.assertNotIn("mesh", result)
        self.assertEqual(before, c.road.vertices.tobytes())

    def test_mutating_an_original_corner_is_not_hidden_by_outer_coarse_parity(self):
        result = self.outputs["reduced"]
        vertices = result["mesh"].vertices.copy()
        vertices[result["rim_profile"]["corner_vertex_ids"][0], 2] += 0.01
        changed = Mesh(vertices, result["mesh"].triangles)
        broken = result | {"mesh": changed, "coarse_mesh": changed}
        with self.assertRaisesRegex(ValueError, "original_dem_corner"):
            qualify_contact(self.context.road, broken)

    def test_original_defaults_still_build_the_same_four_chords(self):
        c = self.context
        result = build_contact(c.road, c.road_dem_z, c.sample_z)
        self.assertEqual(qualify_contact(c.road, result)["outer_edges"], 4)
        self.assertNotIn("rim_profile", result)

    def test_fitted_terrain_preserves_both_existing_source_bound_bridge_walking_surfaces(
        self,
    ):
        from bridge_end_body import build_end_body
        from bridge_end_surface_inputs import actual_end_inputs
        from ground_contact_inputs import ContactContext
        from outshine_ref.geometry import vertical_clearance

        source, index, prepared, ground, _ = actual_end_inputs()
        unknown = source.unknown_nodes
        context = ContactContext(
            ground["surface"],
            prepared[0],
            self.context.terrain,
            np.empty(0),
            ground,
            self.context.metadata,
            0,
        )
        context.road_dem_z = context.sample_z(context.road.vertices[:, :2])
        result = build_rim_contact(
            context.road, context.road_dem_z, context.sample_z, limits=self.limits
        )
        qualify_contact(context.road, result)
        soil = Mesh(
            result["mesh"].vertices,
            result["mesh"].triangles[len(context.road.triangles) :],
        )
        checked = 0
        for value in ground["terminal_values"]:
            cap = value["source_terminal"]
            group = index.groups_by_node[cap["source_node"]]
            if not any(index.index[1][m.edge[:2]].bridge for m in group.members):
                continue
            product, _ = build_end_body(prepared, ground, index, group.identifier)
            clearance = vertical_clearance(soil, product["surface"], seconds=5.0)
            self.assertTrue(clearance["valid"])
            self.assertGreaterEqual(clearance["minimum_clearance_m"], -1e-8)
            checked += 1
        self.assertEqual(checked, 2)
        self.assertEqual(unknown, source.unknown_nodes)


if __name__ == "__main__":
    unittest.main()
