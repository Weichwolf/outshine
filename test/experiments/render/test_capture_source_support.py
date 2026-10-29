"""Independent exact projections and frozen whole-original capture failures."""

import copy
import json
import math
import unittest
from fractions import Fraction
from itertools import product
from pathlib import Path
from unittest.mock import patch

import numpy as np
from capture_source_support import capture_support, translated_bounds
from capture_support_study import plan_support

DATA = (
    Path(__file__).resolve().parents[2]
    / "outshine/data/render/building-field-composition"
)


def camera():
    return {
        "orthographic": True,
        "eye_m": [0, 0, 0],
        "right": [1, 0, 0],
        "up": [0, 1, 0],
        "forward": [0, 0, 1],
        "x_mag_m": 100,
        "y_mag_m": 100,
        "near_m": 0.1,
        "far_m": 1000,
    }


class CaptureSupportTests(unittest.TestCase):
    def test_intervals_enclose_exact_corners_and_interior(self):
        c = camera()
        angle = 0.7
        c.update(
            right=[math.cos(angle), 0, -math.sin(angle)],
            forward=[math.sin(angle), 0, math.cos(angle)],
            eye_m=[1e8, -3e7, 6e7],
        )
        lower, upper = np.array([[-20, -4, 100.0]]), np.array([[30, 12, 190.0]])
        result = capture_support(lower, upper, c)
        for p in [*product(*zip(lower[0], upper[0], strict=True)), (0, 0, 150)]:
            for axis, name in enumerate(("right", "up", "forward")):
                exact = sum(
                    (Fraction(x) - Fraction(e)) * Fraction(a)
                    for x, e, a in zip(p, c["eye_m"], c[name], strict=True)
                )
                self.assertLessEqual(Fraction(result["lower_m"][0, axis]), exact)
                self.assertGreaterEqual(Fraction(result["upper_m"][0, axis]), exact)

    def test_all_geometric_planes_and_boundary_uncertainty_keep_originals(self):
        for low, high in (
            ([-1, -1, 10], [1, 1, 20]),
            ([-101, -1, 10], [-100, 1, 20]),
            ([-1, 100, 10], [1, 101, 20]),
            ([-1, -1, 0], [1, 1, 1]),
            ([-1, -1, 999], [1, 1, 1001]),
        ):
            inside = capture_support(
                np.array([low], float), np.array([high], float), camera()
            )["inside"][0]
            self.assertEqual(bool(inside), low == [-1, -1, 10])
        result = capture_support(
            np.array([[-100.0, 0, 10]]), np.array([[100.0, 0, 10]]), camera()
        )
        self.assertFalse(result["inside"][0])

    def test_translation_encloses_the_actual_stored_float32_vertices(self):
        rng = np.random.default_rng(1720)
        points = rng.uniform(-1000, 1000, (50, 8, 3)).astype(np.float32)
        translation = np.array([16777216.5, -2.1, 0.003])
        low, high = translated_bounds(
            points.min(1).astype(float), points.max(1).astype(float), translation
        )
        actual = (points.astype(float) + translation).astype(np.float32)
        self.assertTrue((low[:, None] <= actual).all())
        self.assertTrue((high[:, None] >= actual).all())

    def test_exact_stored_corner_projections_include_underflow_and_rounding(self):
        signs = np.array(list(product((0, 1), repeat=3)))
        cases = [
            ([-12.1, -5.8, 41.3], [20.2, 14.3, 89.1], [16777216.5, -1.7, 0.003], 0.7),
            ([-1e-45, 0, 1e-45], [1e-45, 1e-45, 2e-45], [0, 0, 0], 1e-300),
        ]
        for lower, upper, offset, angle in cases:
            c = camera()
            c.update(
                right=[math.cos(angle), 0, -math.sin(angle)],
                forward=[math.sin(angle), 0, math.cos(angle)],
            )
            low, high = np.array([lower]), np.array([upper])
            actual = (np.where(signs, high, low) + offset).astype(np.float32)
            result = capture_support(low, high, c, translation=offset)
            for p in actual:
                for axis, name in enumerate(("right", "up", "forward")):
                    exact = sum(
                        Fraction(float(x)) * Fraction(a)
                        for x, a in zip(p, c[name], strict=True)
                    )
                    self.assertLessEqual(Fraction(result["lower_m"][0, axis]), exact)
                    self.assertGreaterEqual(Fraction(result["upper_m"][0, axis]), exact)

    def test_readonly_metadata_is_borrowed_without_mutation(self):
        low, high = np.array([[-1.0, -1, 10]]), np.array([[1.0, 1, 20]])
        low.flags.writeable = high.flags.writeable = False
        result = capture_support(low, high, camera())
        self.assertTrue(result["inside"][0])
        for key in ("lower_m", "upper_m"):
            self.assertFalse(
                np.shares_memory(result[key], low)
                or np.shares_memory(result[key], high)
            )

    def test_frozen_neighbour_requires_a_larger_common_capture(self):
        m = json.loads((DATA / "manifest.json").read_text())
        with np.load(DATA / "captures.npz", allow_pickle=False) as d:
            low, high = d["source_least_m"], d["source_most_m"]
        for name, expected in (
            ("foreground", 0),
            ("separated-background", 710),
            ("interleaved-background", 0),
        ):
            result = capture_support(
                low,
                high,
                m["capture_camera"],
                translation=m["controlled_translations_m"][name][0],
            )
            self.assertEqual(int((~result["inside"]).sum()), expected)
            self.assertTrue(result["finite"].all())
            if name == "separated-background":
                # The exploratory 78-source count considered only side planes.
                # Full containment also includes far clipping: 676 source AABBs.
                outside = (result["lower_m"] < result["capture_lower_m"]) | (
                    result["upper_m"] > result["capture_upper_m"]
                )
                self.assertEqual(outside.sum(0).tolist(), [78, 0, 676])

    def test_whole_cohort_box_accepts_early_but_failure_needs_refinement(self):
        m = json.loads((DATA / "manifest.json").read_text())
        with np.load(DATA / "captures.npz", allow_pickle=False) as d:
            low, high = d["source_least_m"], d["source_most_m"]
        for name, coarse_inside, fine_inside in (
            ("foreground", True, 1891),
            ("separated-background", False, 1181),
            ("interleaved-background", False, 1891),
        ):
            options = {"translation": m["controlled_translations_m"][name][0]}
            coarse = capture_support(
                low.min(0, keepdims=True),
                high.max(0, keepdims=True),
                m["capture_camera"],
                **options,
            )
            fine = capture_support(low, high, m["capture_camera"], **options)
            self.assertEqual(bool(coarse["inside"][0]), coarse_inside)
            self.assertEqual(int(fine["inside"].sum()), fine_inside)
            with patch(
                "capture_support_study.capture_support", wraps=capture_support
            ) as query:
                result, inside, _, count = plan_support(
                    low.min(0, keepdims=True),
                    high.max(0, keepdims=True),
                    low,
                    high,
                    m["capture_camera"],
                    options["translation"],
                )
            self.assertEqual(query.call_count, 1 if coarse_inside else 2)
            self.assertEqual(count, 1 if coarse_inside else 1892)
            self.assertEqual(int(inside.sum()), fine_inside)
            self.assertEqual(len(result["inside"]), 1 if coarse_inside else 1891)

    def test_invalid_or_unbounded_inputs_do_not_become_empty_success(self):
        low, high = np.array([[0.0, 0, 1]]), np.array([[1.0, 1, 2]])
        for c in (
            {**camera(), "orthographic": False},
            {**camera(), "far_m": -1},
            {**camera(), "right": [2, 0, 0]},
        ):
            with self.assertRaises(ValueError):
                capture_support(low, high, c)
        for a, b in ((low[:0], high[:0]), (high, low), (low + math.nan, high)):
            with self.assertRaises(ValueError):
                capture_support(a, b, camera())
        with self.assertRaises(ValueError):
            capture_support(low, high, camera(), bounds_most=0)
        with self.assertRaises(ValueError):
            capture_support(low, high, camera(), translation=(1e300, 0, 0))

    def test_projection_overflow_is_unknown_and_inputs_are_preserved(self):
        c = camera()
        s = math.sqrt(0.5)
        c.update(eye_m=[-1.7e308, -1.7e308, 0], right=[s, s, 0], up=[-s, s, 0])
        low, high = np.array([[0.0, 0, 1]]), np.array([[1.0, 1, 2]])
        old = copy.deepcopy(c)
        result = capture_support(low, high, c)
        self.assertFalse(result["finite"][0])
        self.assertFalse(result["inside"][0])
        self.assertEqual(c, old)
        np.testing.assert_array_equal(low, [[0.0, 0, 1]])


if __name__ == "__main__":
    unittest.main()
