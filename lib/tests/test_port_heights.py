"""Independent public contracts for shared full-width fields and actual finite surfaces."""

import unittest
from dataclasses import FrozenInstanceError, replace

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.transport import (
    PortHeightBudget,
    PortHeightInput,
    PortPlaneField,
    plan_port_heights,
)


def strip(curved=True):
    xy = np.array(
        [[x, y] for x in (-10, 0, 100, 110) for y in (-4, 4)] + [[110, 12], [-10, 12]],
        float,
    )
    faces = np.array(
        [[a, a + 2, a + 1] for a in (0, 2, 4)]
        + [[a + 2, a + 3, a + 1] for a in (0, 2, 4)]
        + [[9, 1, 3], [9, 3, 5], [9, 5, 7], [9, 7, 8]]
    )
    owners = np.r_[np.tile([0, 1, 2], 2), [-1] * 4]
    x, y = xy.T
    z = (
        (np.where(x <= 0, 0.02 * x, 3 - 0.01 * (x - 100)) if curved else 0.02 * x)
        + 0.012 * y
        + 170
    )
    frames = np.tile(np.eye(2) / (2 / 3), (len(faces), 1, 1))
    frames[:6] = np.diag([1 / 0.08, 1 / 0.025])
    frames[[0, 2, 3, 5]] = np.eye(2) / 0.025
    return PortHeightInput(
        Mesh(np.c_[xy, z], faces),
        owners,
        np.array([1, 0, 1]),
        np.array([[2, 3], [4, 5]]),
        np.array([[0, 1], [1, 2]]),
        frames,
    )


def simple_parts(xy, faces, owners):
    xy = np.asarray(xy, float)
    return PortHeightInput(
        Mesh(np.c_[xy, 2 + xy @ [0.01, 0.005]], np.asarray(faces, int)),
        np.asarray(owners, int),
        np.zeros(max(owners) + 1, int),
        np.empty((0, 2), int),
        np.empty((0, 2), int),
        np.tile(np.eye(2) / 0.025, (len(faces), 1, 1)),
    )


def fit(source, **limits):
    return plan_port_heights(
        source, budget=PortHeightBudget(time_limit_s=5, solver_time_limit_s=1, **limits)
    )


def gradients(mesh):
    vertices = mesh.vertices[mesh.triangles]
    normal = np.cross(vertices[:, 1] - vertices[:, 0], vertices[:, 2] - vertices[:, 0])
    return -normal[:, :2] / normal[:, 2:3]


class PortHeights(unittest.TestCase):
    def test_curved_whole_ports_mesh_coverage_and_prior_provenance(self):
        source = strip()
        before = source.surface.vertices.copy()
        result = fit(source)
        self.assertIsNotNone(result.plan, result.report)
        plan = result.plan
        self.assertTrue(result.report["valid"])
        self.assertTrue(
            all(row["status"] == "used" for row in result.report["part_usage"])
        )
        np.testing.assert_array_equal(source.surface.vertices, before)
        np.testing.assert_array_equal(
            plan.mesh.vertices[plan.original_vertex_map, :2], before[:, :2]
        )
        np.testing.assert_array_equal(
            plan.face_parts, source.face_parts[plan.source_faces]
        )
        normalized = np.einsum("nki,ni->nk", plan.grade_frames, gradients(plan.mesh))
        self.assertLessEqual(np.linalg.norm(normalized, axis=1).max(), 1.0000001)
        self.assertLessEqual(plan.height_error_bounds_m.max(), 0.01)
        polygons = shapely.polygons(plan.mesh.vertices[plan.mesh.triangles, :2])
        self.assertTrue(shapely.coverage_is_valid(polygons))
        original = shapely.coverage_union_all(
            shapely.polygons(before[source.surface.triangles, :2])
        )
        self.assertLess(
            shapely.symmetric_difference(
                shapely.coverage_union_all(polygons), original
            ).area,
            1e-10,
        )
        for x, core in ((0, 0), (100, 2)):
            xy = np.c_[np.full(37, x), np.linspace(-4, 4, 37)]
            left = plan.evaluate(xy, np.full(37, 1, int))
            right = plan.evaluate(xy, np.full(37, core, int))
            np.testing.assert_allclose(left[0], right[0], atol=1e-10, rtol=0)
            np.testing.assert_allclose(left[1], right[1], atol=1e-11, rtol=0)
        self.assertLessEqual(
            result.report["audit"]["finite_contacts"][
                "maximum_finite_contact_grade_jump"
            ],
            0.005,
        )

    def test_ownership_cannot_be_changed_through_inputs_or_numpy_flags(self):
        source = strip(False)
        original = source.surface
        caller = Mesh(original.vertices, original.triangles)
        frames = source.grade_frames.copy()
        owned = replace(source, surface=caller, grade_frames=frames)
        caller.vertices.setflags(write=True)
        caller.vertices[:, 2] += 100
        frames[:] = 0
        np.testing.assert_array_equal(owned.surface.vertices, original.vertices)
        result = fit(owned)
        self.assertIsNotNone(result.plan, result.report)
        for array in (
            owned.surface.vertices,
            owned.grade_frames,
            result.plan.mesh.vertices,
            result.plan.mesh.triangles,
            result.plan.plane_coefficients,
            result.plan.field.origins,
        ):
            with self.assertRaises(ValueError):
                array.setflags(write=True)
        with self.assertRaises(FrozenInstanceError):
            result.plan.datum_m = 0
        values, _ = result.plan.evaluate([[50, 0]], [1])
        values[0] += 100
        again, _ = result.plan.evaluate([[50, 0]], [1])
        self.assertAlmostEqual(again[0], 171, places=7)

    def test_source_face_permutation_and_clockwise_winding_keep_actual_ids(self):
        source = strip(False)
        permutation = np.array([6, 0, 7, 1, 8, 2, 9, 3, 4, 5])
        changed = replace(
            source,
            surface=Mesh(
                source.surface.vertices, source.surface.triangles[permutation, ::-1]
            ),
            face_parts=source.face_parts[permutation],
            grade_frames=source.grade_frames[permutation],
        )
        first, second = fit(source), fit(changed)
        self.assertIsNotNone(first.plan, first.report)
        self.assertIsNotNone(second.plan, second.report)
        np.testing.assert_array_equal(
            second.plan.face_parts, changed.face_parts[second.plan.source_faces]
        )
        points = np.array([[10, -3], [47, 2], [95, 0]])
        for a, b in zip(
            first.plan.evaluate(points, [1, 1, 1]),
            second.plan.evaluate(points, [1, 1, 1]),
        ):
            np.testing.assert_allclose(a, b, atol=1e-7, rtol=0)

    def test_rigid_frame_and_world_height_are_preserved(self):
        source = strip()
        angle = 0.7
        rotation = np.array(
            [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
        )
        offset = np.array([1234, -750])
        vertices = source.surface.vertices.copy()
        vertices[:, :2] = vertices[:, :2] @ rotation.T + offset
        moved = replace(
            source,
            surface=Mesh(vertices, source.surface.triangles),
            grade_frames=source.grade_frames @ rotation.T,
        )
        a, b = fit(source), fit(moved)
        self.assertIsNotNone(a.plan, a.report)
        self.assertIsNotNone(b.plan, b.report)
        xy = np.array([[10, 2], [50, -1], [90, 0]])
        za, ga = a.plan.evaluate(xy, [1, 1, 1])
        zb, gb = b.plan.evaluate(xy @ rotation.T + offset, [1, 1, 1])
        np.testing.assert_allclose(za, zb, atol=1e-7, rtol=0)
        np.testing.assert_allclose(ga @ rotation.T, gb, atol=1e-7, rtol=0)

    def test_real_resolvable_gap_is_not_welded(self):
        source = simple_parts(
            [
                [0, 0],
                [1, 0],
                [1, 1],
                [0, 1],
                [1 + 1e-8, 0],
                [2, 0],
                [2, 1],
                [1 + 1e-8, 1],
            ],
            [[0, 1, 2], [0, 2, 3], [4, 5, 6], [4, 6, 7]],
            [0, 0, 1, 1],
        )
        result = fit(source)
        self.assertIsNotNone(result.plan, result.report)
        xy = result.plan.mesh.vertices[result.plan.original_vertex_map, :2]
        np.testing.assert_array_equal(xy, source.surface.vertices[:, :2])
        self.assertNotEqual(
            result.plan.original_vertex_map[1], result.plan.original_vertex_map[4]
        )
        self.assertEqual(
            len(
                shapely.get_parts(
                    shapely.coverage_union_all(
                        shapely.polygons(
                            result.plan.mesh.vertices[result.plan.mesh.triangles, :2]
                        )
                    )
                )
            ),
            2,
        )

    def test_point_contact_has_common_height_and_gradient_without_an_edge(self):
        source = simple_parts(
            [[0, 0], [10, 0], [0, 10], [-10, 0], [0, -10]],
            [[0, 1, 2], [0, 3, 4]],
            [0, 1],
        )
        result = fit(source)
        self.assertIsNotNone(result.plan, result.report)
        za, ga = result.plan.evaluate([[0, 0]], [0])
        zb, gb = result.plan.evaluate([[0, 0]], [1])
        np.testing.assert_allclose(za, zb, atol=1e-7, rtol=0)
        np.testing.assert_allclose(ga, gb, atol=1e-7, rtol=0)

    def test_every_competing_grade_frame_survives_on_a_shared_plane(self):
        source = simple_parts(
            [[0, 0], [1, 0], [1, 1], [0, 1]], [[0, 1, 2], [0, 2, 3]], [0, 0]
        )
        vertices = source.surface.vertices.copy()
        vertices[:, 2] = 2 + 0.06 * vertices[:, :2].sum(axis=1)
        frames = np.array(
            [np.diag([1 / 0.08, 1 / 0.025]), np.diag([1 / 0.025, 1 / 0.08])]
        )
        source = replace(
            source,
            surface=Mesh(vertices, source.surface.triangles),
            grade_frames=frames,
        )
        result = fit(source)
        self.assertIsNotNone(result.plan, result.report)
        np.testing.assert_array_equal(
            result.plan.grade_frames, frames[result.plan.source_faces]
        )
        values = np.einsum(
            "nki,ni->nk", result.plan.grade_frames, gradients(result.plan.mesh)
        )
        self.assertLessEqual(np.linalg.norm(values, axis=1).max(), 1.0000001)

    def test_rounding_may_not_silently_remove_an_original_source_face(self):
        source = simple_parts(
            [[0, 0], [1, 0], [0.5, 1e-15], [0.5, -1e-15]],
            [[0, 1, 2], [1, 0, 3]],
            [0, 0],
        )
        result = fit(source)
        self.assertIsNone(result.plan)
        self.assertEqual(result.report["reason"], "port_original_face_unresolved")
        self.assertEqual(result.report["part_usage"][0]["status"], "not_used")

    def test_work_limits_produce_a_reason_for_every_original_part(self):
        source = strip()
        limits = [
            {"time_limit_s": 0},
            {"solver_time_limit_s": 0},
            {"max_vertices": 1},
            {"max_faces": 1},
            {"max_parts": 1},
            {"max_ports": 1},
            {"max_output_vertices": 1},
            {"max_output_faces": 1},
            {"max_stations_per_part": 2},
            {"max_cones": 1},
            {"max_variables": 1},
            {"max_queries": 1},
            {"max_sparse_nonzeros": 1},
        ]
        for values in limits:
            with self.subTest(values=values):
                result = plan_port_heights(source, budget=PortHeightBudget(**values))
                self.assertIsNone(result.plan)
                self.assertFalse(result.report["valid"])
                self.assertEqual(len(result.report["part_usage"]), 3)
                self.assertTrue(
                    all(
                        row["reason"] and row["status"] == "not_used"
                        for row in result.report["part_usage"]
                    )
                )

    def test_weld_candidates_are_bounded_before_pairs_are_materialized(self):
        source = simple_parts(
            [
                [0, 0],
                [1, 0],
                [1, 1],
                [0, 1],
                [1 + 1e-15, 0],
                [2, 0],
                [2, 1],
                [1 + 1e-15, 1],
            ],
            [[0, 1, 2], [0, 2, 3], [4, 5, 6], [4, 6, 7]],
            [0, 0, 1, 1],
        )
        result = fit(source, max_weld_pairs=1)
        self.assertIsNone(result.plan)
        self.assertEqual(result.report["reason"], "slice_weld_pair_budget")

    def test_invalid_numeric_field_domains_and_budget_counts_are_rejected(self):
        field = PortPlaneField(
            [[0, 0], [100, 0]],
            [[0, 1]],
            [[[0, 0], [100, 0]]],
            [[[1, 0], [-1, 0]]],
            [100],
        )
        for values in (
            {"scales": [0]},
            {"scales": [float("nan")]},
            {"plane_ids": [[0.5, 1]]},
            {"inward_normals": [[[2, 0], [-1, 0]]]},
        ):
            with self.subTest(values=list(values)), self.assertRaises(ValueError):
                replace(field, **values)
        with self.assertRaises(ValueError):
            field.bounds(
                [[[1e300, 0], [0, 1], [0, 0]]], [0], [0, 0, 0, 3, 0, 0], np.eye(2)[None]
            )
        for values in (
            {"max_queries": 0},
            {"max_iterations": True},
            {"max_rounds": 0},
            {"solver_time_limit_s": float("nan")},
            {"time_limit_s": -1},
        ):
            with self.subTest(values=values), self.assertRaises(ValueError):
                PortHeightBudget(**values)

    def test_invalid_integer_ids_frames_and_actual_port_neighbours_are_rejected(self):
        source = strip()
        for values in (
            {"face_parts": source.face_parts + 0.5},
            {"port_edges": source.port_edges + 0.5},
            {"port_parts": source.port_parts + 0.5},
            {"grade_frames": np.zeros_like(source.grade_frames)},
            {"face_parts": np.full_like(source.face_parts, -1)},
        ):
            with self.subTest(values=list(values)), self.assertRaises(ValueError):
                replace(source, **values)
        neighbours = source.port_parts.copy()
        neighbours[0, 0] = 2
        with self.assertRaisesRegex(ValueError, "actual adjacent faces"):
            fit(replace(source, port_parts=neighbours))

    def test_progress_observer_cannot_change_acceptance(self):
        seen = []

        def observer(row):
            seen.append(row)
            if row.get("fitting"):
                row["fitting"]["solver"]["valid"] = False
            if "quality" in row:
                row["quality"]["whole_world_valid"] = False

        result = plan_port_heights(
            strip(), budget=PortHeightBudget(solver_time_limit_s=1), progress=observer
        )
        self.assertIsNotNone(result.plan, result.report)
        self.assertTrue(seen)
        self.assertTrue(result.report["audit"]["whole_world_valid"])

    def test_small_conservative_polygon_solver_obeys_the_same_actual_mesh_bounds(self):
        result = plan_port_heights(
            strip(), budget=PortHeightBudget(solver_time_limit_s=1), method="lp"
        )
        self.assertIsNotNone(result.plan, result.report)
        normalized = np.einsum(
            "nki,ni->nk", result.plan.grade_frames, gradients(result.plan.mesh)
        )
        self.assertLessEqual(np.linalg.norm(normalized, axis=1).max(), 1.0000001)
        self.assertLessEqual(result.plan.height_error_bounds_m.max(), 0.01)
        self.assertLessEqual(
            result.report["audit"]["finite_contacts"][
                "maximum_finite_contact_grade_jump"
            ],
            0.005,
        )

    def test_owned_field_rejects_fractional_ids_and_checks_whole_triangle_bounds(self):
        field = PortPlaneField(
            [[0, 0], [100, 0]],
            [[0, 1]],
            [[[0, 0], [100, 0]]],
            [[[1, 0], [-1, 0]]],
            [100],
        )
        coefficients = [0, 0, 0, 3, 0, 0]
        triangle = np.array([[[0, -4], [100, -4], [100, 4]]])
        bound, error = field.bounds(triangle, [0], coefficients, np.eye(2)[None])
        self.assertGreater(bound[0], 0.04)
        self.assertGreater(error[0], 0.01)
        for owner in ([0.5], [True], [-1], [99]):
            with self.subTest(owner=owner), self.assertRaises(ValueError):
                field.evaluate([[50, 0]], owner, coefficients)
        with self.assertRaises(ValueError):
            field.origins.setflags(write=True)
        with self.assertRaises(ValueError):
            replace(field, max_queries=1).bounds(
                triangle, [0], coefficients, np.eye(2)[None]
            )


if __name__ == "__main__":
    unittest.main()
