"""Whole affine overlap extrema, source widths and actual exported bridge bodies."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from elevated_rail_audit import inspect
from elevated_rail_inputs import add_surface, sources
from elevated_rail_joints import apply as apply_joints
from elevated_rail_joints import inspect as inspect_joints
from elevated_rail_joints import joints
from elevated_rail_plane import fit_plane, lower_constraints
from outshine_ref.contracts import Mesh
from shared_bridge_geometry import layout
from stair_module_inputs import FIXTURE
from surface_solid_methods import close_surface


class RaisedRailTests(unittest.TestCase):
    def setUp(self):
        self.domain = shapely.box(-1, -1, 1, 1)
        self.axes = np.array([[1.0, 0]])
        self.lower = Mesh(
            np.array([[-2, -2, 0], [2, -2, 0], [2, 2, 0], [-2, 2, 0]], float),
            np.array([[0, 1, 2], [0, 2, 3]]),
        )

    def test_every_clipped_affine_corner_has_the_independent_required_height(self):
        vertices = self.lower.vertices.copy()
        vertices[:, 2] = 2 * vertices[:, 0] + 3 * vertices[:, 1] + 7
        values, _ = lower_constraints(
            Mesh(vertices, self.lower.triangles), self.domain, 2.5
        )
        np.testing.assert_allclose(
            values[:, 2], values[:, :2] @ [2, 3] + 9.5, atol=1e-13
        )
        self.assertLessEqual(abs(values[:, :2]).max(), 1)

    def test_interior_terrain_peak_is_not_lost_by_only_testing_source_corners(self):
        vertices = np.vstack((self.lower.vertices, [0, 0, 9]))
        faces = np.array([[0, 1, 4], [1, 2, 4], [2, 3, 4], [3, 0, 4]])
        values, _ = lower_constraints(Mesh(vertices, faces), self.domain, 2.5)
        self.assertEqual(float(values[:, 2].max()), 11.5)
        self.assertTrue(np.any(np.all(values == [0, 0, 11.5], axis=1)))

    def test_hole_boundaries_are_used_and_no_positive_overlap_produces_no_points(self):
        domain = self.domain.difference(shapely.box(-0.5, -0.5, 0.5, 0.5))
        values, _ = lower_constraints(self.lower, domain, 3)
        self.assertFalse(np.any((abs(values[:, :2]) < 0.5).all(axis=1)))
        values, _ = lower_constraints(self.lower, shapely.box(10, 10, 11, 11), 3)
        self.assertEqual(values.shape, (0, 3))

    def test_constraint_capacity_precedes_coordinate_expansion(self):
        with (
            patch(
                "elevated_rail_plane.shapely.get_coordinates",
                side_effect=AssertionError("expanded"),
            ),
            self.assertRaisesRegex(ValueError, "before expansion"),
        ):
            lower_constraints(self.lower, self.domain, 3, points_most=1)

    def test_float32_lower_height_is_a_constraint_even_when_double_is_lower(self):
        vertices = self.lower.vertices.copy()
        vertices[:, 2] = 1000.00004
        lower = Mesh(vertices, self.lower.triangles)
        points, groups, notes = [], [], []
        add_surface(
            points,
            groups,
            notes,
            lower,
            self.domain,
            0.62,
            2.5,
            "lower",
            "terrain",
            65536,
        )
        _, coefficients, _ = fit_plane(
            np.vstack(points), self.domain, self.axes, method="flat"
        )
        self.assertAlmostEqual(
            coefficients[2], float(np.float32(1000.00004)) + 3.12, places=12
        )
        self.assertGreater(coefficients[2], 1000.00004 + 3.12)
        self.assertEqual(len(groups), 1)

    def test_cached_rail_is_declined_before_redecoding_sources(self):
        import json

        fixture = FIXTURE.with_name("stair-elevated-rail-vienna.json")
        key = json.loads(fixture.read_text())["source_ids"][0]
        with (
            patch(
                "elevated_rail_inputs.source_context",
                side_effect=AssertionError("decoded"),
            ),
            self.assertRaisesRegex(ValueError, "unique whole new"),
        ):
            sources(fixture, [key])

    def test_affine_fit_improves_area_mean_without_violating_any_whole_corner(self):
        xy = np.array([[-1, -1], [1, -1], [1, 1], [-1, 1]], float)
        values = np.c_[xy, xy @ [0.01, -0.02] + 4]
        origin, coefficients, fitted = fit_plane(values, self.domain, self.axes)
        _, _, flat = fit_plane(values, self.domain, self.axes, method="flat")
        np.testing.assert_allclose(origin, [0, 0], atol=1e-14)
        np.testing.assert_allclose(coefficients, [0.01, -0.02, 4], atol=1e-12)
        self.assertAlmostEqual(fitted["area_mean_height_m"], 4)
        self.assertAlmostEqual(flat["area_mean_height_m"], 4.03)

    def test_flat_candidate_uses_no_lp_or_design_matrix(self):
        points = np.array([[0.0, 0, 3], [1, 0, 4]])
        with patch("elevated_rail_plane.linprog", side_effect=AssertionError("solver")):
            _, coefficients, report = fit_plane(
                points, self.domain, self.axes, method="flat"
            )
        np.testing.assert_array_equal(coefficients, [0, 0, 4])
        self.assertEqual(report["design_bytes"], 0)

    def test_anisotropic_profile_limits_are_actual_constraints(self):
        values = np.array([[-1, 0, 0], [1, 0, 1], [0, -1, 0], [0, 1, 0]], float)
        _, coefficients, _ = fit_plane(values, self.domain, self.axes)
        self.assertLessEqual(abs(coefficients[0]), 0.04 + 1e-12)
        self.assertLessEqual(abs(coefficients[1]), 0.03 + 1e-12)

    def test_invalid_fit_inputs_and_unbounded_profile_work_are_declined(self):
        for values in (np.empty((0, 3)), np.full((3, 3), np.nan), np.ones((65537, 3))):
            with self.assertRaisesRegex(ValueError, "Bounded finite"):
                fit_plane(values, self.domain, self.axes)
        with self.assertRaisesRegex(ValueError, "Nondegenerate"):
            fit_plane(np.ones((3, 3)), self.domain, np.zeros((1, 2)))
        with self.assertRaisesRegex(ValueError, "local affine"):
            fit_plane(np.array([[10000, 0, 3]]), self.domain, self.axes)

    def corner_rows(self, angle=0.356):
        radians = np.deg2rad(angle)
        return [
            {
                "source_id": "incoming",
                "xy": np.array([[-10.0, 0], [0, 0]]),
                "width_m": 3.8,
                "source_grid_m": 2**-11,
                "end_heights_m": [3.12, 3.12],
                "original_mvt_xy": [[-10, 0], [0, 0]],
            },
            {
                "source_id": "outgoing",
                "xy": np.array(
                    [[0.0, 0], [10 * np.cos(radians), 10 * np.sin(radians)]]
                ),
                "width_m": 3.8,
                "source_grid_m": 2**-11,
                "end_heights_m": [3.12, 3.12],
                "original_mvt_xy": [[0, 0], [10, 1]],
            },
        ]

    def test_flat_cap_corner_loses_whole_profile_even_when_union_is_closed(self):
        rows = self.corner_rows()
        _, domain, _ = layout(rows)
        section = shapely.LineString(joints(rows)[0]["section"])
        self.assertGreater(section.difference(domain).length, 1.4)
        with self.assertRaisesRegex(ValueError, "Whole shared"):
            inspect_joints(rows, domain)
        apply_joints(rows)
        top, domain, _ = layout(rows)
        self.assertEqual(inspect_joints(rows, domain)[0]["uncovered_length_m"], 0)
        body = close_surface(top, 0.6)
        proof = inspect(body, top.triangles, rows, [], 0.6, 0.04, 0.03)
        self.assertTrue(
            all(p["joint_profiles"][0]["uncovered_length_m"] == 0 for p in proof)
        )

    def test_straight_joint_needs_no_added_pad(self):
        rows = self.corner_rows(0)
        apply_joints(rows)
        self.assertTrue(all("junction_pads_xy_m" not in row for row in rows))
        _, domain, _ = layout(rows)
        self.assertEqual(inspect_joints(rows, domain)[0]["uncovered_length_m"], 0)

    def test_sharp_or_three_way_rail_joints_remain_explicitly_unsupported(self):
        with self.assertRaisesRegex(ValueError, "bounded simple joint"):
            apply_joints(self.corner_rows(60))
        rows = self.corner_rows()
        rows.append({**rows[1], "source_id": "third"})
        with self.assertRaisesRegex(ValueError, "two-source"):
            apply_joints(rows)
        self.assertTrue(all("junction_pads_xy_m" not in row for row in rows))

    def body(self):
        rows = [
            {
                "source_id": "original-rail",
                "original_mvt_xy": [[0, 0], [1, 0]],
                "xy": np.array([[-1.0, 0], [1, 0]]),
                "width_m": 0.5,
                "source_grid_m": 2**-11,
                "end_heights_m": [3.12, 3.12],
            }
        ]
        surface, _, _ = layout(rows)
        return close_surface(surface, 0.6), surface.triangles, rows

    def test_independent_actual_body_source_and_both_precision_clearances(self):
        body, faces, rows = self.body()
        proof = inspect(
            body, faces, rows, [("terrain", self.lower, 2.5)], 0.6, 0.04, 0.03
        )
        self.assertEqual(len(proof), 2)
        self.assertTrue(all(p["maximum_normalized_profile"] == 0 for p in proof))
        self.assertTrue(
            all(p["clearances"][0]["minimum_clearance_m"] > 2.5 for p in proof)
        )

    def test_refused_alternative_does_not_discard_the_valid_flat_candidate(self):
        from elevated_rail_study import compare

        body, faces, rows = self.body()
        n = len(body.vertices) // 2
        top = Mesh(body.vertices[:n], faces)
        domain = shapely.union_all(shapely.polygons(top.vertices[top.triangles, :2]))
        points = np.c_[top.vertices[:, :2], np.full(n, 3.12)]
        recipe = {"deck_thickness_m": 0.6, "along_grade": 0.04, "cross_grade": 0.03}
        with patch(
            "elevated_rail_study.inspect",
            side_effect=[[{"actual": True}], ValueError("Float32 alternate refused")],
        ):
            result, arrays = compare(recipe, rows, top, domain, points, [])
        self.assertTrue(result["flat"]["accepted"])
        self.assertFalse(result["tilted"]["accepted"])
        self.assertIn("Float32", result["tilted"]["reason"])
        self.assertIn("flat_vertices", arrays)

    def test_actual_rail_joint_retains_the_float32_tilt_counterexample(self):
        recipe, _, _, rows, top, _, _ = sources(
            FIXTURE.with_name("stair-elevated-rail-vienna.json"), []
        )
        # Fixed input witness, not an expected solver solution or generated oracle.
        origin = np.array([166.06239050857826, 94.14096877467338])
        coefficients = np.array(
            [-0.01423530526232489, 0.020366154359393134, 168.75090398182496]
        )
        vertices = top.vertices.copy()
        vertices[:, 2] = (vertices[:, :2] - origin) @ coefficients[:2] + coefficients[2]
        body = close_surface(Mesh(vertices, top.triangles), recipe["deck_thickness_m"])
        with self.assertRaisesRegex(ValueError, "float32=True"):
            inspect(
                body,
                top.triangles,
                rows,
                [],
                recipe["deck_thickness_m"],
                recipe["along_grade"],
                recipe["cross_grade"],
            )

    def test_actual_bottom_and_too_little_whole_clearance_are_rejected(self):
        body, faces, rows = self.body()
        with self.assertRaisesRegex(ValueError, "clearance failed"):
            inspect(body, faces, rows, [("terrain", self.lower, 2.6)], 0.6, 0.04, 0.03)
        with self.assertRaisesRegex(ValueError, "thickness"):
            inspect(body, faces, rows, [], 0.61, 0.04, 0.03)

    def test_missing_source_width_is_not_accepted_as_a_closed_deck(self):
        body, faces, rows = self.body()
        rows[0]["width_m"] = 1
        with self.assertRaisesRegex(ValueError, "whole original source width"):
            inspect(body, faces, rows, [], 0.6, 0.04, 0.03)

    def test_actual_profile_violation_does_not_pass_only_because_body_is_closed(self):
        body, faces, rows = self.body()
        vertices = body.vertices.copy()
        vertices[:, 2] += 0.1 * vertices[:, 0]
        # Rebuild the actual paired lower surface without separate rounding noise.
        n = len(vertices) // 2
        vertices[n:] = vertices[:n] - [0, 0, 0.6]
        with self.assertRaisesRegex(ValueError, "profile exceeded"):
            inspect(Mesh(vertices, body.triangles), faces, rows, [], 0.6, 0.04, 0.03)


if __name__ == "__main__":
    unittest.main()
