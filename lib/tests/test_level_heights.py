"""Flat-group identity, real Float32 failure, bounded absence and importable core."""

import hashlib
import json
import unittest
from itertools import pairwise
from pathlib import Path
from unittest.mock import patch

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport import HeightBudget, fit_surface_heights
from outshine_ref.transport.level_heights import fit_level_heights


def strip():
    xy = np.array([[x, y] for x in range(5) for y in (0, 1)], float)
    faces = np.array(
        [[2 * x, 2 * x + 2, 2 * x + 1] for x in range(4)]
        + [[2 * x + 1, 2 * x + 2, 2 * x + 3] for x in range(4)]
    )
    heights = np.where(xy[:, 0] <= 1, 100, np.where(xy[:, 0] >= 3, 110, 105))
    return (
        Mesh(np.c_[xy, heights], faces),
        np.broadcast_to(np.eye(2) / 0.2, (len(faces), 2, 2)).copy(),
        [np.arange(4), np.arange(6, 10)],
    )


def whole_input():
    path = (
        Path(__file__).parents[2]
        / "test/outshine/data/transport/flat-group-height-input.npz"
    )
    meta = json.loads(path.with_suffix(".json").read_text())
    if hashlib.sha256(path.read_bytes()).hexdigest() != meta["input_sha256"]:
        raise ValueError("versioned whole real height input changed")
    with np.load(path, allow_pickle=False) as data:
        offsets = data["group_offsets"]
        groups = [data["group_vertices"][a:b] for a, b in pairwise(offsets)]
        return Mesh(data["vertices"], data["faces"]), data["grade_frames"], groups


class LevelHeights(unittest.TestCase):
    def check(self, source, result, frames, groups):
        self.assertIsNotNone(result.mesh)
        self.assertTrue(result.report["valid"])
        mesh = result.mesh
        np.testing.assert_array_equal(mesh.vertices[:, :2], source.vertices[:, :2])
        np.testing.assert_array_equal(mesh.triangles, source.triangles)
        np.testing.assert_array_equal(
            mesh.vertices, mesh.vertices.astype("f4").astype(float)
        )
        for group in groups:
            np.testing.assert_array_equal(
                mesh.vertices[group, 2], np.full(len(group), mesh.vertices[group[0], 2])
            )
        triangles = mesh.vertices[mesh.triangles]
        normals = np.cross(
            triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        )
        gradient = -normals[:, :2] / normals[:, 2:3]
        self.assertLessEqual(
            np.linalg.norm(np.einsum("fij,fj->fi", frames, gradient), axis=1).max(), 1
        )
        with self.assertRaises(ValueError):
            mesh.vertices[0, 2] = 0
        with self.assertRaises(TypeError):
            result.report["valid"] = False

    def test_free_flat_levels_resolve_inconsistent_procedural_means(self):
        source, frames, groups = strip()
        pins = {int(i): float(source.vertices[i, 2]) for group in groups for i in group}
        self.assertIsNone(fit_surface_heights(source, frames, fixed=pins).mesh)
        before = source.vertices.copy()
        result = fit_level_heights(source, frames, groups)
        self.check(source, result, frames, groups)
        error = float(abs(result.mesh.vertices[:, 2] - source.vertices[:, 2]).max())
        # Two metres at grade .2 allow only .4 m between means ten metres apart.
        self.assertGreaterEqual(error, (10 - 0.4) / 2 - 1e-7)
        self.assertLessEqual(error, 4.80001)
        np.testing.assert_array_equal(source.vertices, before)

    def test_whole_real_input_preserves_every_face_and_exact_group(self):
        source, frames, groups = whole_input()
        result = fit_level_heights(source, frames, groups)
        self.check(source, result, frames, groups)
        self.assertEqual((len(source.vertices), len(source.triangles)), (520, 765))
        self.assertLessEqual(
            abs(result.mesh.vertices[:, 2] - source.vertices[:, 2]).max(), 0.172
        )
        self.assertLessEqual(result.report["rounding"]["candidates"], 100)
        self.assertLessEqual(result.report["rounding"]["maximum_change_m"], 0.005)

    def test_already_valid_exact_positions_do_no_solver_work(self):
        source, frames, groups = strip()
        flat = Mesh(
            np.c_[source.vertices[:, :2], np.full(len(source.vertices), 100.0)],
            source.triangles,
        )
        with patch(
            "outshine_ref.transport.level_heights._continuous.solve_groups",
            side_effect=AssertionError("unnecessary solve"),
        ):
            result = fit_level_heights(flat, frames, groups)
        self.assertIs(result.mesh, flat)
        self.assertEqual(result.report["solver_status"], "prior_feasible")
        self.assertEqual(result.report["iterations"], 0)
        self.check(flat, result, frames, groups)

    def test_budget_absence_and_declared_maxima(self):
        source, frames, groups = strip()
        for budget in (
            HeightBudget(time_limit_s=0),
            HeightBudget(max_vertices=1),
            HeightBudget(max_faces=1),
            HeightBudget(max_iterations=1),
        ):
            result = fit_level_heights(source, frames, groups, budget=budget)
            self.assertIsNone(result.mesh)
            self.assertFalse(result.report["valid"])
        for budget in (HeightBudget(time_limit_s=9), HeightBudget(max_iterations=101)):
            with self.assertRaises(ValueError):
                fit_level_heights(source, frames, groups, budget=budget)

    def test_group_identity_and_bounded_shape_contracts(self):
        source, frames, _groups = strip()
        bad = (
            [],
            [[0, 1]],
            [[0, 1, 1]],
            [[-1, 0, 1]],
            [[0, 1, 10]],
            [[0.0, 1.0, 2.0]],
            [[False, True, False]],
            [[0, 1, 2], [2, 3, 4]],
            [[0, 1, 2]] * 21,
            [np.array(1)],
            [np.zeros((3, 3), int)],
            [[[0], [1], [2]]],
            [np.array([0, 1, 2**64 - 1], np.uint64)],
        )
        for groups in bad:
            with self.subTest(groups=str(groups)[:60]), self.assertRaises(ValueError):
                fit_level_heights(source, frames, groups)

    def test_bad_frame_shapes_and_nonfinite_frames_are_rejected(self):
        source, frames, groups = strip()
        for value in (
            np.zeros_like(frames),
            frames * np.nan,
            frames[:1],
            np.broadcast_to(np.eye(2), (1000000, 2, 2)),
            np.ones(frames.shape, complex),
        ):
            with self.assertRaises(ValueError):
                fit_level_heights(source, value, groups)

    def test_solver_claim_does_not_admit_invalid_group_or_face_heights(self):
        source, frames, groups = strip()
        for heights in (
            np.arange(len(source.vertices)) * 1000.0,
            np.full(len(source.vertices), np.nan),
        ):
            answer = (heights, {"solver_status": "Solved", "iterations": 1})
            with patch(
                "outshine_ref.transport.level_heights._continuous.solve_groups",
                return_value=answer,
            ):
                result = fit_level_heights(source, frames, groups)
            self.assertIsNone(result.mesh)
            self.assertFalse(result.report["valid"])

    def test_unsupported_position_encoding_is_absent_before_solver_work(self):
        source, frames, groups = strip()
        for vertices, reason in (
            (source.vertices + np.array([0.1, 0, 0]), "source_XY_not_exact_Float32"),
            (source.vertices * [1, 1, 1e40], "height_outside_Float32"),
        ):
            with patch(
                "outshine_ref.transport.level_heights._continuous.solve_groups",
                side_effect=AssertionError("unsupported solve"),
            ):
                result = fit_level_heights(
                    Mesh(vertices, source.triangles), frames, groups
                )
            self.assertIsNone(result.mesh)
            self.assertEqual(result.report["reason"], reason)

    def test_exact_coordinate_rotation_and_translation_preserve_constraints(self):
        source, frames, groups = strip()
        rotation = np.array([[0.0, -1], [1, 0]])
        vertices = source.vertices.copy()
        vertices[:, :2] = vertices[:, :2] @ rotation.T + [2**20, -(2**20)]
        transformed = Mesh(vertices, source.triangles)
        actual_frames = frames @ rotation.T
        result = fit_level_heights(transformed, actual_frames, groups)
        self.check(transformed, result, actual_frames, groups)
        self.assertLessEqual(
            abs(result.mesh.vertices[:, 2] - transformed.vertices[:, 2]).max(), 4.80001
        )


if __name__ == "__main__":
    unittest.main()
