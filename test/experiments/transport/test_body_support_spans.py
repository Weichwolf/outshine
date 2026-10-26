"""Body-derived one-sided support, actual step fronts and qualified topology."""

import copy
import json
import unittest

import numpy as np
from body_support_spans import support_points, trace
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from world_stair_reference import FIXTURE, construct


def flat_square():
    points = np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]], dtype=float)
    faces = np.array([[0, 1, 2], [0, 2, 3]])
    return {
        "body_vertices": points,
        "top_faces": faces,
        "reference_vertices": points,
        "reference_faces": faces,
        "face_sources": np.ones(2, dtype=np.uint64),
        "face_step": np.full(2, -1),
        "levels_m": np.empty(0),
    }


class BodySupportSpans(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.capture = json.loads(FIXTURE.read_text())
        cls.case = construct(cls.capture)
        cls.body = dict(cls.case["data"])
        cls.body["body_vertices"] = cls.body["body_vertices"].astype(np.float32)
        cls.path = np.array(
            [r[:2] for r in cls.capture["stations_east_north_grade_node"]]
        )
        cls.forward = trace(cls.body, cls.path, 7)
        cls.backward = trace(cls.body, cls.path[::-1], 7)

    def test_actual_world_caps_cover_the_whole_source_path(self):
        r = self.forward
        self.assertEqual(r["work"]["support_spans"], 48)
        for segment in range(3):
            bounds = r["parameter_bounds"][r["source_segment_ids"] == segment]
            self.assertEqual(bounds[0, 0], 0)
            self.assertEqual(bounds[-1, 1], 1)
            np.testing.assert_array_equal(bounds[1:, 0], bounds[:-1, 1])
        np.testing.assert_array_equal(
            r["station_bounds_m"][1:, 0], r["station_bounds_m"][:-1, 1]
        )
        np.testing.assert_array_equal(
            r["source_masks"], self.body["face_sources"][r["body_cap_face_ids"]]
        )

    def test_all_midspan_heights_are_on_the_actual_float32_body(self):
        r = self.forward
        endpoints = support_points(r)
        midpoint = endpoints.mean(axis=1)
        sampler = SurfaceSampler(
            Mesh(self.body["body_vertices"], self.body["top_faces"]), xy_tolerance_m=0
        )
        sampled = sampler.sample(midpoint[:, :2])
        self.assertFalse(any(sampled.reasons))
        np.testing.assert_array_equal(sampled.face_indices, r["body_cap_face_ids"])
        np.testing.assert_allclose(
            midpoint[:, 2], sampled.points_enu_m[:, 2], atol=1e-7, rtol=0
        )
        xy = (
            self.path[r["source_segment_ids"], None]
            + r["parameter_bounds"][:, :, None]
            * np.diff(self.path, axis=0)[r["source_segment_ids"], None]
        )
        # Reconstruction residual only: the existing metric height tolerance is unchanged.
        np.testing.assert_allclose(endpoints[:, :, :2], xy, atol=1e-7, rtol=0)

    def test_actual_riser_jumps_are_not_smoothed(self):
        r = self.forward
        self.assertEqual(r["connections"].count("body_step"), 21)
        steps = np.array(r["connections"]) == "body_step"
        self.assertTrue(np.all(r["height_jumps_m"][steps] > 0.17))
        self.assertTrue(np.all(r["height_jumps_m"][steps] < 0.179))
        np.testing.assert_array_equal(r["height_jumps_m"][~steps], 0)
        points = support_points(r)
        np.testing.assert_array_equal(
            r["height_jumps_m"][steps], (points[1:, 0, 2] - points[:-1, 1, 2])[steps]
        )

    def test_previous_ramp_priors_remain_a_negative_control(self):
        points = support_points(self.forward)
        priors = np.array(
            [r[2] for r in self.capture["stations_east_north_grade_node"]]
        )
        actual = [
            points[np.flatnonzero(self.forward["source_segment_ids"] == i)[-1], 1, 2]
            for i in (0, 1)
        ]
        self.assertGreater(abs(actual[0] - priors[1]), 0.06)
        self.assertGreater(abs(actual[1] - priors[2]), 0.38)

    def test_reversal_keeps_the_physical_support_and_reverses_steps(self):
        np.testing.assert_array_equal(
            self.backward["body_cap_face_ids"], self.forward["body_cap_face_ids"][::-1]
        )
        np.testing.assert_allclose(
            support_points(self.backward),
            support_points(self.forward)[::-1, ::-1],
            atol=1e-7,
            rtol=0,
        )
        np.testing.assert_allclose(
            self.backward["height_jumps_m"],
            -self.forward["height_jumps_m"][::-1],
            atol=1e-7,
            rtol=0,
        )

    def test_scalar_and_batch_are_exactly_the_same_support(self):
        for path, expected in (
            (self.path, self.forward),
            (self.path[::-1], self.backward),
        ):
            actual = trace(self.body, path, 7, strategy="scalar")
            for key, value in expected.items():
                if isinstance(value, np.ndarray):
                    self.assertEqual(actual[key].dtype, value.dtype)
                    self.assertEqual(actual[key].tobytes(), value.tobytes())
            self.assertEqual(actual["connections"], expected["connections"])
            self.assertEqual(
                actual["work"]["face_intersections"],
                expected["work"]["face_intersections"],
            )
            self.assertEqual(actual["work"]["intersection_dispatches"], 174)
            self.assertEqual(expected["work"]["intersection_dispatches"], 3)

    def test_true_shared_indexed_edge_has_one_support(self):
        body = flat_square()
        result = trace(body, np.array([[0.1, 0.1], [0.9, 0.9]]), 1)
        self.assertEqual(len(result["body_cap_face_ids"]), 1)
        self.assertEqual(int(result["body_cap_face_ids"][0]), 0)

    def test_owned_float64_body_also_uses_its_own_caps(self):
        for path in (self.path, self.path[::-1]):
            batch = trace(self.case["data"], path, 7)
            scalar = trace(self.case["data"], path, 7, strategy="scalar")
            self.assertEqual(batch["connections"].count("body_step"), 21)
            self.assertIs(batch["body"], self.case["data"])
            for key, value in batch.items():
                if isinstance(value, np.ndarray):
                    self.assertEqual(value.tobytes(), scalar[key].tobytes())

    def test_coincident_independent_sheets_are_ambiguous_even_at_equal_height(self):
        body = flat_square()
        for key in ("body_vertices", "reference_vertices"):
            body[key] = np.vstack([body[key], body[key]])
        for key in ("top_faces", "reference_faces"):
            body[key] = np.vstack([body[key], body[key] + 4])
        body["face_sources"] = np.array([1, 1, 2, 2], dtype=np.uint64)
        body["face_step"] = np.full(4, -1)
        path = np.array([[0.1, 0.2], [0.8, 0.9]])
        with self.assertRaisesRegex(ValueError, "ambiguous"):
            trace(body, path, 3)
        result = trace(body, path, 1)
        np.testing.assert_array_equal(result["source_masks"], 1)

    def test_unshared_reference_sheets_do_not_join_by_xy_contact(self):
        body = flat_square()
        for key in ("body_vertices", "reference_vertices"):
            body[key] = np.vstack([body[key], body[key]])
        for key in ("top_faces", "reference_faces"):
            body[key] = body[key].copy()
            body[key][1] += 4
        with self.assertRaisesRegex(ValueError, "disconnected"):
            trace(body, np.array([[0.1, 0.8], [0.8, 0.1]]), 1)

    def test_missing_coverage_or_ownership_is_not_zero_height(self):
        with self.assertRaises(ValueError):
            trace(self.body, self.path, 1)
        with self.assertRaises(ValueError):
            trace(self.body, self.path, 8)
        with self.assertRaisesRegex(ValueError, "unknown source bits"):
            trace(self.body, self.path, 15)

    def test_highest_source_bit_remains_an_unsigned_identity(self):
        body = flat_square()
        body["face_sources"] = np.full(2, 1 << 63, dtype=np.uint64)
        result = trace(body, np.array([[0.1, 0.2], [0.8, 0.9]]), 1 << 63)
        np.testing.assert_array_equal(result["source_masks"], np.uint64(1 << 63))

    def test_budget_cancellation_and_consistency_failures_preserve_inputs(self):
        before = {k: a.tobytes() for k, a in self.body.items()}
        before_path = self.path.tobytes()
        calls = iter([False, False, True])
        for options in (
            {"max_faces": 0},
            {"max_face_tests": 1},
            {"max_spans": 1},
            {"cancelled": lambda: next(calls)},
        ):
            with self.subTest(options=options), self.assertRaises(ValueError):
                trace(self.body, self.path, 7, **options)
        self.assertIs(self.forward["body"], self.body)
        self.assertTrue(np.shares_memory(self.forward["path_xy_m"], self.path))
        self.assertEqual(before_path, self.path.tobytes())
        for key, words in before.items():
            self.assertEqual(self.body[key].tobytes(), words)
        invalid = copy.deepcopy(self.body)
        invalid["face_step"][0] = 0
        with self.assertRaisesRegex(ValueError, "flat levels"):
            trace(invalid, self.path, 7)


if __name__ == "__main__":
    unittest.main()
