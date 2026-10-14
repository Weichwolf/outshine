"""Saved-source coverage, untouched input and full rotated-ribbon countercases."""

import hashlib
import unittest

import numpy as np
from outshine_ref.transport import plan_vertical_transition, sample_vertical_transition
from surface_height_checks import normal_gradients
from vertical_transition_experiment import FIXTURE, compare_sources, ribbon


class SavedTransitions(unittest.TestCase):
    def test_same_saved_sources_keep_contacts_and_declare_local_length_failures(self):
        before = hashlib.sha256(FIXTURE.read_bytes()).hexdigest()
        report = compare_sources()
        self.assertEqual(len(report["cases"]), 256)
        self.assertEqual(before, report["fixture_sha256"])
        self.assertEqual(before, hashlib.sha256(FIXTURE.read_bytes()).hexdigest())
        counts = {
            method: sum(r["methods"][method]["valid"] for r in report["cases"])
            for method in ("parabolic", "cubic")
        }
        self.assertEqual(counts, {"parabolic": 246, "cubic": 229})
        self.assertEqual(len({r["source_id"] for r in report["cases"]}), 256)
        for row in report["cases"]:
            for method in row["methods"].values():
                if method["valid"]:
                    self.assertLessEqual(
                        method["maximum_normalized_face_grade"], 1.0000001
                    )
                    self.assertLessEqual(method["sampled_chord_error_m"], 0.0100001)
                    self.assertLessEqual(method["endpoint_error_m"], 1e-7)
                    self.assertLessEqual(
                        method["endpoint_chord_grade_jump"],
                        np.sqrt(2 * method["maximum_second_derivative_per_m"] * 0.01)
                        + 1e-10,
                    )
                else:
                    self.assertEqual(method["reason"], "insufficient_profile_length")
                    self.assertGreater(
                        method["minimum_transition_length_m"], method["length_m"]
                    )

    def test_actual_crossfall_consumes_elliptic_budget_on_rotated_wide_ribbons(self):
        for angle in (0.0, 0.7, 1.9):
            direction = np.array([np.cos(angle), np.sin(angle)])
            frame = np.array(
                [direction / 0.08, [-direction[1] / 0.025, direction[0] / 0.025]]
            )
            for delta in (-6.0, 6.0):
                profile, _ = plan_vertical_transition(
                    163.0, 163.0 + delta, 160.0, grade=0.08 * 0.8
                )
                table, _ = sample_vertical_transition(profile, height_error_m=0.001)
                mesh = ribbon(np.array([700000.0, -400000.0]), direction, table, 30.0)
                gradients = normal_gradients(
                    mesh.vertices[:, :2], mesh.triangles, mesh.vertices[:, 2]
                )
                self.assertLessEqual(
                    np.linalg.norm(gradients @ frame.T, axis=1).max(), 1.0000001
                )
                np.testing.assert_allclose(
                    gradients @ np.array([-direction[1], direction[0]]),
                    0.015,
                    atol=1e-9,
                )
                self.assertTrue(
                    (
                        np.cross(
                            mesh.vertices[mesh.triangles[:, 1]]
                            - mesh.vertices[mesh.triangles[:, 0]],
                            mesh.vertices[mesh.triangles[:, 2]]
                            - mesh.vertices[mesh.triangles[:, 0]],
                        )[:, 2]
                        > 0
                    ).all()
                )


if __name__ == "__main__":
    unittest.main()
