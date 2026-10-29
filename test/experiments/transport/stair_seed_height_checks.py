"""Stored-height failures repair without moving source XY or loosening profiles."""

import copy
import json
import unittest
import warnings
from pathlib import Path

import numpy as np
from stair_height_contraction import MAX_REPAIR_M, correct, normalized_grade
from stair_seed_height_rounding import free_seed, repair, verify


def real_face():
    path = (
        Path(__file__).parents[2]
        / "outshine/data/transport/open-ground-rounding-face.json"
    )
    record = json.loads(path.read_text())
    return tuple(
        np.asarray(record[key]) for key in ("vertices", "faces", "grade_frames")
    )


def repaired_data():
    vertices, faces, frames = real_face()
    result, _ = repair(vertices, faces, frames, seconds=8)
    data = {
        "surface_vertices": result,
        "surface_faces": faces,
        "unrounded_seed_heights": vertices[:, 2].copy(),
        "fixed": np.zeros(len(vertices), bool),
    }
    rows = [{"xy": np.array([[0.0, 0], [40, 0]])}]
    return data, frames, rows


class StoredSeedHeights(unittest.TestCase):
    def test_actual_source_face_passes_both_precisions_without_xy_change(self):
        vertices, faces, frames = real_face()
        original = vertices.copy()
        self.assertLess(normalized_grade(vertices, faces, frames, False), 1)
        self.assertGreater(normalized_grade(vertices, faces, frames, True), 1)
        result, proof = repair(vertices, faces, frames, seconds=8)
        np.testing.assert_array_equal(vertices, original)
        np.testing.assert_array_equal(result[:, :2], vertices[:, :2])
        self.assertLessEqual(abs(result[:, 2] - vertices[:, 2]).max(), MAX_REPAIR_M)
        self.assertLessEqual(len(proof["trials"]), 7)
        for cast in (False, True):
            self.assertLessEqual(normalized_grade(result, faces, frames, cast), 1)

    def test_already_valid_heights_keep_the_same_object_without_a_witness(self):
        vertices, faces, frames = real_face()
        vertices[:, 2] = 162.0
        result, proof = repair(vertices, faces, frames, seconds=0)
        self.assertIs(result, vertices)
        self.assertIsNone(proof)

    def test_exhausted_and_invalid_time_budgets_do_not_change_input(self):
        vertices, faces, frames = real_face()
        original = vertices.copy()
        for seconds in (0, -1, 8.01, np.inf, np.nan):
            with self.subTest(seconds=seconds), self.assertRaises(ValueError):
                repair(vertices, faces, frames, seconds=seconds)
            np.testing.assert_array_equal(vertices, original)

    def test_invalid_continuous_fit_is_not_rescued_by_contraction(self):
        vertices, faces, frames = real_face()
        vertices[:, 2] = 162 + 10 * (vertices[:, 2] - 162)
        with self.assertRaisesRegex(ValueError, "valid original Double"):
            repair(vertices, faces, frames, seconds=8)

    def test_inexact_xy_and_float32_overflow_are_not_repaired(self):
        vertices, faces, frames = real_face()
        changed = vertices.copy()
        changed[:, 0] += 1e-8
        with self.assertRaisesRegex(ValueError, "exact Float32 XY"):
            repair(changed, faces, frames, seconds=8)
        for value in (1e39, np.inf, np.nan):
            changed = vertices.copy()
            changed[0, 2] = value
            with warnings.catch_warnings():
                warnings.simplefilter("error")
                with self.subTest(value=value), self.assertRaises(ValueError):
                    repair(changed, faces, frames, seconds=8)

    def test_saved_witness_checks_original_validity_budget_and_actual_result(self):
        data, frames, rows = repaired_data()
        proof = verify(data, frames, rows, [])
        self.assertLessEqual(proof["maximum_change_m"], MAX_REPAIR_M)
        excessive = copy.deepcopy(data)
        excessive["surface_vertices"][:, 2] += 0.01
        with self.assertRaisesRegex(ValueError, "5 mm budget"):
            verify(excessive, frames, rows, [])
        invalid = copy.deepcopy(data)
        invalid["surface_vertices"][:, 2] = invalid["unrounded_seed_heights"]
        with self.assertRaisesRegex(ValueError, "actual D/Q"):
            verify(invalid, frames, rows, [])
        malformed = copy.deepcopy(data)
        malformed["unrounded_seed_heights"] = malformed[
            "unrounded_seed_heights"
        ].astype("f4")
        with self.assertRaisesRegex(ValueError, "original Double"):
            verify(malformed, frames, rows, [])

    def test_pins_multi_source_junctions_and_rings_keep_their_own_contracts(self):
        data, frames, rows = repaired_data()
        self.assertTrue(free_seed(rows, [], data["fixed"]))
        for altered_rows, plans, fixed in (
            (rows * 2, [], data["fixed"]),
            (rows, [{"junction": True}], data["fixed"]),
            (rows, [], np.array([True, False, False])),
            ([{"xy": np.array([[0.0, 0], [1, 0], [0, 0]])}], [], data["fixed"]),
        ):
            self.assertFalse(free_seed(altered_rows, plans, fixed))
            altered = dict(data, fixed=fixed)
            with self.assertRaisesRegex(ValueError, "unpinned open seed"):
                verify(altered, frames, altered_rows, plans)

    def test_generic_contraction_honors_its_remaining_budget(self):
        vertices, faces, frames = real_face()
        with self.assertRaisesRegex(ValueError, "time budget"):
            correct(vertices, faces, frames, seconds=0)


if __name__ == "__main__":
    unittest.main()
