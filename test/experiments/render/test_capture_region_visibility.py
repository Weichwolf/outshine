"""Independent segment intersections and disocclusion controls for #844."""

import unittest

import numpy as np
from capture_region_candidates import propose
from capture_region_visibility import covered_from_ball


def blocked(blocker, origins, points):
    """Independent ray/triangle barycentric reader, with segment t in (0,1)."""
    a, b, c = blocker
    direction = points - origins
    systems = np.stack(
        [
            np.broadcast_to(b - a, direction.shape),
            np.broadcast_to(c - a, direction.shape),
            -direction,
        ],
        axis=2,
    )
    uv_t = np.linalg.solve(systems, (origins - a)[..., None])[..., 0]
    return (
        (uv_t[:, 0] >= 0)
        & (uv_t[:, 1] >= 0)
        & (uv_t[:, :2].sum(1) <= 1)
        & (uv_t[:, 2] > 0)
        & (uv_t[:, 2] < 1)
    )


class RegionVisibilityTests(unittest.TestCase):
    def setUp(self):
        self.blocker = np.array([[-12, -10, 10], [12, -10, 10], [0, 14, 10.0]])
        self.target = np.array([[-1, -1, 30], [1, -1, 30], [0, 1, 30.0]])
        self.eye = np.zeros(3)

    def test_whole_ball_certificate_against_independent_segments(self):
        result = covered_from_ball(self.blocker, self.target, self.eye, 6, opaque=True)
        self.assertTrue(result["covered"], result)
        self.assertEqual(result["exact_plane_tests"], 13)
        rng = np.random.default_rng(844)
        direction = rng.normal(size=(4096, 3))
        eyes = 6 * direction / np.linalg.norm(direction, axis=1)[:, None]
        weights = rng.dirichlet([1, 1, 1], len(eyes))
        self.assertTrue(blocked(self.blocker, eyes, weights @ self.target).all())

    def test_point_capture_omission_is_not_a_region_certificate(self):
        # Thin triangle just inside the right silhouette at z=30.
        target = np.array([[17, -6, 30], [17.25, -6, 30], [17, -5.75, 30.0]])
        self.assertTrue(
            covered_from_ball(self.blocker, target, self.eye, 0, opaque=True)["covered"]
        )
        self.assertFalse(
            covered_from_ball(self.blocker, target, self.eye, 6, opaque=True)["covered"]
        )
        # A six-metre move exposes the previously entirely hidden triangle.
        eyes = np.broadcast_to([6.0, 0, 0], target.shape)
        self.assertFalse(blocked(self.blocker, eyes, target).all())

    def test_winding_and_common_rigid_translation_do_not_change_result(self):
        shift = np.array([1e6, -2e6, 3e6])
        self.assertTrue(
            covered_from_ball(
                self.blocker[::-1] + shift,
                self.target + shift,
                self.eye + shift,
                6,
                opaque=True,
            )["covered"]
        )

    def test_plane_contact_transparency_and_foreground_are_retained(self):
        self.assertFalse(
            covered_from_ball(self.blocker, self.target, self.eye, 6)["covered"]
        )
        self.assertFalse(
            covered_from_ball(self.blocker, self.target, self.eye, 10, opaque=True)[
                "covered"
            ]
        )
        front = self.target.copy()
        front[:, 2] = 5
        self.assertFalse(
            covered_from_ball(self.blocker, front, self.eye, 0, opaque=True)["covered"]
        )

    def test_culled_backface_cannot_occlude_an_observer_ball(self):
        self.assertFalse(
            covered_from_ball(
                self.blocker,
                self.target,
                self.eye,
                6,
                opaque=True,
                cull_backfaces=True,
            )["covered"]
        )
        self.assertTrue(
            covered_from_ball(
                self.blocker[::-1],
                self.target,
                self.eye,
                6,
                opaque=True,
                cull_backfaces=True,
            )["covered"]
        )

    def test_invalid_and_degenerate_inputs_do_not_certify_occlusion(self):
        with self.assertRaises(ValueError):
            covered_from_ball(self.blocker, self.target, self.eye, -1, opaque=True)
        with self.assertRaises(ValueError):
            covered_from_ball(
                self.blocker.astype(np.float32), self.target, self.eye, 1, opaque=True
            )
        degenerate = np.zeros((3, 3))
        self.assertFalse(
            covered_from_ball(degenerate, self.target, self.eye, 1, opaque=True)[
                "covered"
            ]
        )

    def test_projected_proposal_selects_nearest_front_blocker(self):
        near = self.target.copy()
        near[:, 2] = 5
        triangles = np.array([self.blocker[::-1], self.target[::-1], near[::-1]])
        answer, report = propose(triangles, self.eye, np.eye(3), pair_budget=3)
        self.assertEqual(answer[1], 2)
        self.assertEqual(answer[2], -1)
        self.assertLessEqual(report["maximum_batch_pairs"], 3)
        self.assertTrue(
            blocked(
                triangles[answer[1]], self.eye[None], triangles[1].mean(0)[None]
            ).all()
        )
        triangles[2] = triangles[2, ::-1]
        answer, _ = propose(triangles, self.eye, np.eye(3))
        self.assertEqual(answer[1], 0)

    def test_proposal_budget_and_near_plane_retain_unknown_geometry(self):
        triangles = np.array([self.blocker[::-1], self.target[::-1]])
        answer, report = propose(triangles, self.eye, np.eye(3), pair_budget=1)
        self.assertTrue((answer == -1).all())
        self.assertEqual(report["reason"], "candidate_pair_budget_too_small")
        answer, report = propose(triangles, self.eye, np.eye(3), total_pair_budget=1)
        self.assertTrue((answer == -1).all())
        self.assertEqual(report["reason"], "total_candidate_budget_exhausted")
        self.assertEqual(report["candidate_pairs"], 0)
        triangles[0, 0, 2] = 0
        answer, report = propose(triangles, self.eye, np.eye(3))
        self.assertTrue((answer == -1).all())
        self.assertEqual(report["reason"], "geometry_crosses_proposal_near_plane")


if __name__ == "__main__":
    unittest.main()
