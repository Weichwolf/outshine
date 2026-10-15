"""Feasible tilted roads retain their shape at unconstrained terminal ends."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport import PortHeightBudget, PortHeightInput, plan_port_heights


def terminal(junction=True, gradient=(0.02, 0.012)):
    xy = np.array([[x, y] for x in (-10, 0, 100) for y in (-4, 4)], float)
    faces = np.array([[0, 2, 1], [2, 4, 3], [2, 3, 1], [4, 5, 3]])
    parts = np.array([0, 1, 0, 1]) if junction else np.zeros(4, int)
    frames = np.tile(np.diag([1 / 0.08, 1 / 0.025]), (4, 1, 1))
    if junction:
        frames[parts == 0] = np.eye(2) / 0.025
    return PortHeightInput(
        Mesh(np.c_[xy, 170 + xy @ gradient], faces),
        parts,
        np.array([1, 0]) if junction else np.array([0]),
        np.array([[2, 3]]) if junction else np.empty((0, 2), int),
        np.array([[0, 1]]) if junction else np.empty((0, 2), int),
        frames,
    )


class AffinePorts(unittest.TestCase):
    def test_one_port_and_isolated_corridors_preserve_every_height_and_gradient(self):
        points = np.array([[x, y] for x in (0, 17, 50, 83, 100) for y in (-4, 0, 4)])
        for junction in (True, False):
            for offset, angle in (([0, 0], 0), ([12345, -6789], 0.7)):
                with self.subTest(junction=junction, offset=offset):
                    source = terminal(junction)
                    rotation = np.array(
                        [
                            [np.cos(angle), -np.sin(angle)],
                            [np.sin(angle), np.cos(angle)],
                        ]
                    )
                    vertices = source.surface.vertices.copy()
                    vertices[:, :2] = vertices[:, :2] @ rotation.T + offset
                    source = replace(
                        source,
                        surface=Mesh(vertices, source.surface.triangles),
                        grade_frames=source.grade_frames @ rotation.T,
                    )
                    result = plan_port_heights(source)
                    self.assertIsNotNone(result.plan, result.report)
                    plan = result.plan
                    height, gradient = plan.evaluate(
                        points @ rotation.T + offset,
                        np.full(len(points), int(junction)),
                    )
                    np.testing.assert_allclose(
                        height, 170 + points @ [0.02, 0.012], atol=1e-10, rtol=0
                    )
                    np.testing.assert_allclose(
                        gradient,
                        np.tile(np.array([0.02, 0.012]) @ rotation.T, (len(points), 1)),
                        atol=1e-12,
                        rtol=0,
                    )
                    np.testing.assert_allclose(
                        plan.mesh.vertices[plan.original_vertex_map],
                        vertices,
                        atol=1e-10,
                        rtol=0,
                    )
                    self.assertEqual(len(plan.mesh.vertices), len(vertices))
                    self.assertEqual(
                        len(plan.mesh.triangles), len(source.surface.triangles)
                    )
                    self.assertEqual(
                        result.report["rounds"][0]["fitting"]["solver"]["method"],
                        "direct",
                    )

    def test_affine_prior_that_exceeds_a_junction_grade_is_still_constrained(self):
        source = terminal(gradient=(0.06, 0.012))
        result = plan_port_heights(source)
        self.assertIsNotNone(result.plan, result.report)
        self.assertNotEqual(
            result.report["rounds"][0]["fitting"]["solver"]["method"], "direct"
        )
        corners = result.plan.mesh.vertices[result.plan.mesh.triangles]
        normal = np.cross(corners[:, 1] - corners[:, 0], corners[:, 2] - corners[:, 0])
        gradient = -normal[:, :2] / normal[:, 2:3]
        normalized = np.einsum("nki,ni->nk", result.plan.grade_frames, gradient)
        self.assertLessEqual(np.linalg.norm(normalized, axis=1).max(), 1.0000001)

    def test_affine_direct_path_still_obeys_output_variable_and_query_limits(self):
        for limits in (
            {"max_output_vertices": 1},
            {"max_output_faces": 1},
            {"max_variables": 1},
            {"max_queries": 1},
        ):
            with self.subTest(limits=limits):
                result = plan_port_heights(
                    terminal(), budget=PortHeightBudget(**limits)
                )
                self.assertIsNone(result.plan)
                self.assertTrue(
                    all(row["reason"] for row in result.report["part_usage"])
                )

    def test_many_touching_affine_parts_need_no_sparse_assembly(self):
        xy = np.array([[x, y] for y in range(7) for x in range(7)], float)
        faces = np.array(
            [
                triangle
                for y in range(6)
                for x in range(6)
                for triangle in (
                    (7 * y + x, 7 * y + x + 1, 7 * (y + 1) + x),
                    (7 * y + x + 1, 7 * (y + 1) + x + 1, 7 * (y + 1) + x),
                )
            ]
        )
        source = PortHeightInput(
            Mesh(np.c_[xy, 170 + xy @ [0.01, 0.005]], faces),
            np.arange(len(faces)),
            np.zeros(len(faces), int),
            np.empty((0, 2), int),
            np.empty((0, 2), int),
            np.tile(np.eye(2) / 0.025, (len(faces), 1, 1)),
        )
        with patch(
            "outshine_ref.transport.port_heights._system.local_matrix",
            side_effect=AssertionError(
                "a feasible global plane needs no sparse assembly"
            ),
        ):
            result = plan_port_heights(
                source, budget=PortHeightBudget(max_sparse_nonzeros=1)
            )
        self.assertIsNotNone(result.plan, result.report)
        np.testing.assert_array_equal(
            result.plan.mesh.vertices[result.plan.original_vertex_map],
            source.surface.vertices,
        )
        points = np.array([[0.2, 0.2], [5.7, 5.2]])
        height, gradient = result.plan.evaluate(points, [0, 70])
        np.testing.assert_allclose(
            height, 170 + points @ [0.01, 0.005], atol=1e-11, rtol=0
        )
        np.testing.assert_allclose(gradient, [[0.01, 0.005]] * 2, atol=1e-12, rtol=0)
        self.assertEqual(
            result.report["rounds"][0]["fitting"]["system"]["sparse_nonzeros"], 0
        )

    def test_roundoff_sized_prior_residual_is_preserved_and_included_in_chord_bounds(
        self,
    ):
        source = terminal()
        vertices = source.surface.vertices.copy()
        vertices[0, 2] += 1e-12
        source = replace(source, surface=Mesh(vertices, source.surface.triangles))
        result = plan_port_heights(source)
        self.assertIsNotNone(result.plan, result.report)
        plan = result.plan
        self.assertEqual(
            result.report["rounds"][0]["fitting"]["solver"]["method"], "direct"
        )
        np.testing.assert_array_equal(
            plan.mesh.vertices[plan.original_vertex_map], vertices
        )
        weights = np.array([[1, 0, 0], [0, 1, 0], [0, 0, 1], [0.2, 0.3, 0.5]])
        corners = plan.mesh.vertices[plan.mesh.triangles]
        xy = np.einsum("ki,nij->nkj", weights, corners[:, :, :2])
        finite_height = np.einsum("ki,ni->nk", weights, corners[:, :, 2])
        analytic_height, _ = plan.evaluate(
            xy.reshape(-1, 2), np.repeat(plan.face_parts, len(weights))
        )
        error = abs(analytic_height.reshape(finite_height.shape) - finite_height).max(
            axis=1
        )
        self.assertTrue(np.all(error <= plan.height_error_bounds_m + 1e-14))


if __name__ == "__main__":
    unittest.main()
