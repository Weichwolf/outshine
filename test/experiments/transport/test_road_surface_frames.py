"""Independent differential, handedness and LOD controls for #1531."""

import unittest

import numpy as np
from compare_road_frames import sample_normals
from road_surface_frames import attributes, differentials, triangle_normals
from road_surface_lod import prepare
from road_surface_model import Curve
from test_road_surface_lod import recipe


class RoadFrameTests(unittest.TestCase):
    def test_zero_shoulder_chart_has_no_division_by_duplicate_edges(self):
        curve = Curve(recipe())
        plan = prepare(curve, 0.25)
        frames = attributes(curve, plan["stations_m"])
        sampled = sample_normals(
            curve,
            plan,
            frames["normals_enh"],
            np.array([0, 5, 10]),
            np.array([-2, 0, 2]),
        )
        np.testing.assert_array_equal(sampled, np.broadcast_to([0, 0, 1], (3, 3, 3)))

    def test_constant_grade_bank_closed_form(self):
        source = recipe()
        for pose in (*source["seams"], source["intervals"][0]["middle"]):
            pose[3], pose[7], pose[9] = 0.3 * pose[0], 0.3, 0.2
        curve = Curve(source)
        got = attributes(curve, [0, 5, 10])
        expected = np.array([-0.3, np.tan(0.2), 1.0])
        expected /= np.linalg.norm(expected)
        np.testing.assert_allclose(
            got["normals_enh"], np.broadcast_to(expected, (3, 4, 3)), atol=3e-8
        )
        np.testing.assert_array_equal(
            got["station_lateral_m"][:, :, 0], np.array([[0] * 4, [5] * 4, [10] * 4])
        )

    def test_changing_bank_curvature_and_height_finite_difference(self):
        curve = Curve(recipe(curvature=0.05, height=0.015, bank_rate=0.025))
        s = np.array([0.1, 2.3, 6.7, 9.9])
        ds, du = differentials(curve, s)
        epsilon = 1e-5
        observed = (curve.at(s + epsilon) - curve.at(s - epsilon)) / (2 * epsilon)
        np.testing.assert_allclose(ds, observed, atol=1e-9, rtol=1e-9)
        observed_u = (curve.at(s)[:, 2] - curve.at(s)[:, 1]) / (2 * curve.half_width)
        np.testing.assert_allclose(du[:, 1], observed_u, atol=1e-14)

    def test_frame_handedness_and_orthogonality(self):
        curve = Curve(recipe(curvature=-0.07, height=-0.02, bank_rate=-0.03))
        stations = [0, 1, 7, 10]
        got = attributes(curve, stations)
        normal, tangent = got["normals_enh"], got["tangents_enh"][:, :, :3]
        np.testing.assert_allclose(np.linalg.norm(normal, axis=2), 1, atol=1e-7)
        np.testing.assert_allclose(np.sum(normal * tangent, axis=2), 0, atol=5e-8)
        bitangent = np.cross(normal, tangent)
        _, du = differentials(curve, stations)
        self.assertTrue(np.all(np.sum(bitangent * du, axis=2) > 0))
        self.assertTrue(np.all(normal[:, :, 2] > 0))
        np.testing.assert_array_equal(got["tangents_enh"][:, :, 3], 1)

    def test_common_station_attributes_are_bit_identical_across_lods(self):
        curve = Curve(recipe(height=0.03, bank_rate=0.025))
        coarse, fine = prepare(curve, 0.25), prepare(curve, 0.005)
        a, b = (
            attributes(curve, coarse["stations_m"]),
            attributes(curve, fine["stations_m"]),
        )
        indices = np.searchsorted(fine["stations_m"], coarse["stations_m"])
        np.testing.assert_array_equal(fine["stations_m"][indices], coarse["stations_m"])
        for name in a:
            np.testing.assert_array_equal(a[name], b[name][indices])
        ca, used = triangle_normals(coarse)
        fb, _ = triangle_normals(fine)
        self.assertGreater(np.max(np.linalg.norm(ca - fb[indices], axis=2)[used]), 0.01)

    def test_folds_between_requested_stations_are_rejected(self):
        curve = Curve(recipe(curvature=0.5))
        with self.assertRaisesRegex(ValueError, "folded"):
            attributes(curve, [0, 10])
        for stations in ([-1], [11], [np.nan], [[0]]):
            with self.assertRaises(ValueError):
                attributes(Curve(recipe()), stations)

    def test_source_seams_are_explicitly_one_sided(self):
        source = recipe(knots=3)
        source["intervals"][1]["middle"][5] = 0.1
        source["intervals"][1]["max_abs_curvature_per_m"] = 0.1
        curve = Curve(source)
        # Add grade so curvature changes the longitudinal slope of offset lanes.
        curve.height[:, :] = np.array([[0, 0.5, 1, 1.5], [1.5, 2, 2.5, 3]])
        left = attributes(curve, [5], side="left")
        right = attributes(curve, [5], side="right")
        self.assertGreater(
            np.max(abs(left["normals_enh"] - right["normals_enh"])), 0.02
        )
        with self.assertRaises(ValueError):
            attributes(curve, [5], side="average")

    def test_tiny_actual_shape_requires_evaluation_between_vertices(self):
        source = recipe()
        length, bank = 0.005, 0.033
        source["range_m"] = [0, length]
        source["intervals"][0]["range_m"] = [0, length]
        for pose, s in zip(
            (*source["seams"], source["intervals"][0]["middle"]),
            (0, length, length / 2),
            strict=True,
        ):
            pose[:4] = [s, s, 0, 0]
            t = s / length
            pose[9] = bank * (3 * t * t - 2 * t * t * t)
            pose[10] = bank * (6 * t - 6 * t * t) / length
        curve = Curve(source)
        end = attributes(curve, [0, length])["normals_enh"]
        middle = attributes(curve, [length / 2], lateral_m=[-2, 0, 2])["normals_enh"]
        self.assertGreater(np.min(end[:, :, 2]), 0.99)
        q = np.tan(bank / 2)
        q_rate = 1.5 * bank / length / np.cos(bank / 2) ** 2
        expected_up = 1 / np.sqrt(1 + q * q + (2 * q_rate) ** 2)
        np.testing.assert_allclose(middle[:, [0, 2], 2], expected_up, atol=1e-8)
        self.assertGreater(middle[0, 1, 2], 0.99)
        ds, _ = differentials(curve, [length / 2], lateral_m=[-2, 0, 2])
        epsilon = 1e-8
        observed = (
            curve.at(np.array([length / 2 + epsilon]))
            - curve.at(np.array([length / 2 - epsilon]))
        ) / (2 * epsilon)
        np.testing.assert_allclose(
            ds[:, [0, 2]], observed[:, [1, 2]], atol=2e-8, rtol=2e-8
        )
        with self.assertRaises(ValueError):
            attributes(curve, [0], lateral_m=[3])

    def test_finite_extreme_grade_does_not_overflow_frame_normalization(self):
        source = recipe()
        curve = Curve(source)
        curve.height[:] = [0, 1e200, 2e200, 3e200]
        got = attributes(curve, [0, 5, 10])
        self.assertTrue(np.isfinite(got["normals_enh"]).all())
        self.assertTrue(np.isfinite(got["tangents_enh"]).all())


if __name__ == "__main__":
    unittest.main()
