"""Quantized constructive levels, bounded absence and actual Float32 countercases."""

import json
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from stair_closed_height_rounding import normalized_grade
from stair_joint_height_checks import strip
from stair_joint_height_fit import level_groups
from stair_quantized_joint_heights import fit_quantized_levels, lattice


class QuantizedJointHeights(unittest.TestCase):
    def test_captured_whole_face_is_double_valid_but_requires_quantization_planning(
        self,
    ):
        path = (
            Path(__file__).parents[2]
            / "outshine/data/transport/joint-ground-height-quantization.json"
        )
        saved = json.loads(path.read_text())
        old = np.array(saved["vertices_before_m"])
        new = np.array(saved["vertices_lattice_m"])
        frame = np.array(saved["reserved_grade_frame"])[None]
        face = np.array([[0, 1, 2]])
        for cast, key in ((False, "expected_before_D"), (True, "expected_before_Q")):
            self.assertAlmostEqual(
                normalized_grade(old, face, frame, cast), saved[key], places=8
            )
            self.assertLessEqual(normalized_grade(new, face, frame, cast), 1)
        self.assertLess(saved["expected_before_D"], 1)
        self.assertGreater(saved["expected_before_Q"], 1)
        np.testing.assert_array_equal(new[:, :2], old[:, :2])
        np.testing.assert_array_equal(new, new.astype("f4").astype(float))

    def test_actual_milp_fits_flat_levels_and_all_double_float32_faces(self):
        surface, plans, frames = strip()
        result, levels = fit_quantized_levels(surface, frames, plans)
        self.assertIsNotNone(result.mesh)
        vertices = result.mesh.vertices
        np.testing.assert_array_equal(vertices, vertices.astype("f4").astype(float))
        self.assertLessEqual(abs(vertices[:, 2] - surface.vertices[:, 2]).max(), 4.805)
        self.assertFalse(result.report["optimality_claimed"])
        for group, level in zip(
            level_groups(vertices[:, :2], plans), levels, strict=True
        ):
            np.testing.assert_array_equal(
                vertices[group, 2], np.full(len(group), level)
            )
        for cast in (False, True):
            self.assertLessEqual(
                normalized_grade(vertices, surface.triangles, frames, cast), 1
            )

    def test_budget_and_cross_binade_absence_are_explicit(self):
        surface, plans, frames = strip()
        result, levels = fit_quantized_levels(surface, frames, plans, time_limit_s=0)
        self.assertIsNone(result.mesh)
        self.assertIsNone(levels)
        for target in (np.array([63, 65]), np.array([0, 0]), np.array([-1, 1])):
            with self.assertRaisesRegex(ValueError, "binade"):
                lattice(target)
        for kwargs in ({"time_limit_s": 9}, {"max_nodes": 101}, {"max_nodes": True}):
            with self.assertRaises(ValueError):
                fit_quantized_levels(surface, frames, plans, **kwargs)

    def test_solver_success_does_not_admit_an_invalid_integer_or_face_candidate(self):
        surface, plans, frames = strip()
        for values in (
            np.full(len(surface.vertices) + 1, 1e9),
            np.arange(len(surface.vertices) + 1) * 1000,
        ):
            answer = SimpleNamespace(
                status=0, message="claimed success", x=values, mip_node_count=1
            )
            with patch("scipy.optimize.milp", return_value=answer):
                result, _levels = fit_quantized_levels(surface, frames, plans)
            self.assertIsNone(result.mesh)


if __name__ == "__main__":
    unittest.main()
