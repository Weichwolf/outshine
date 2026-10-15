"""Feasible tilted roads retain their shape at unconstrained terminal ends."""

import unittest
from dataclasses import replace

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

    def test_affine_direct_path_still_obeys_output_variable_query_and_sparse_limits(
        self,
    ):
        for limits in (
            {"max_output_vertices": 1},
            {"max_output_faces": 1},
            {"max_variables": 1},
            {"max_queries": 1},
            {"max_sparse_nonzeros": 1},
        ):
            with self.subTest(limits=limits):
                result = plan_port_heights(
                    terminal(), budget=PortHeightBudget(**limits)
                )
                self.assertIsNone(result.plan)
                self.assertTrue(
                    all(row["reason"] for row in result.report["part_usage"])
                )


if __name__ == "__main__":
    unittest.main()
