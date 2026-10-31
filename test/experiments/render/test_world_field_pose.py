"""World mapping enclosure against an independent exact geometric projection."""

import json
import math
import unittest
from fractions import Fraction as F
from pathlib import Path

import numpy as np
from field_pose_geometry import geometric_candidate
from field_pose_geometry import projection_bound as local_bound
from field_view_projection import CapturePlane
from test_field_pose_geometry import perspective
from world_field_pose import clip_from_field, projection_bound

ROOT = Path(__file__).resolve().parents[3]


def exact_error(plane, p, frame, eye, basis, P, raster, normal=None):
    def v(a):
        return tuple(F(float(x)) for x in a)

    def dot(a, b):
        return sum(x * y for x, y in zip(a, b, strict=True))

    p, c, d, n = (
        v(p),
        v(plane.centre),
        v(plane.toward_eye),
        v(plane.toward_eye if normal is None else normal),
    )
    t = dot(tuple(a - b for a, b in zip(p, c, strict=True)), n) / dot(d, n)
    q = tuple(a - t * b for a, b in zip(p, d, strict=True))
    clips = []
    for point in (p, q):
        world = tuple(dot(v(row), (*point, F(1))) for row in frame[:3])
        relative = tuple(x - y for x, y in zip(world, v(eye), strict=True))
        camera = (*[dot(v(axis), relative) for axis in basis], F(1))
        clips.append(tuple(dot(v(row), camera) for row in P))
    return tuple(
        abs(F(size, 2) * (clips[0][i] / clips[0][3] - clips[1][i] / clips[1][3]))
        for i, size in enumerate(raster)
    )


def placement(lat, lon):
    p, l = np.deg2rad([lat, lon])
    e = np.array([-np.sin(l), np.cos(l), 0.0])
    u = np.array([np.cos(p) * np.cos(l), np.cos(p) * np.sin(l), np.sin(p)])
    n = np.cross(u, e)
    R = np.column_stack([e, u, -n])
    M = np.eye(4)
    M[:3, :3] = R
    M[:3, 3] = 6378137 * u
    return M


class WorldGeometry(unittest.TestCase):
    def setUp(self):
        self.plane = CapturePlane((-12, -4, -8), (18, 9, 11), (1, 2, 1), (0, 0, 1))
        self.basis = np.diag([1.0, 1.0, -1.0])
        self.raster = (1280, 720)
        self.P = perspective(focal=700)

    def test_exact_world_projection_under_geographic_placements_and_motion(self):
        rng = np.random.default_rng(1881)
        for lat, lon in ((0, 0), (55, 179.9), (-42, -179.9), (89.9, 40), (-89.9, -75)):
            M = placement(lat, lon)
            R = M[:3, :3]
            for local_eye in ((0, 0, 2000), (120, -40, 64000), (90, 20, 240000)):
                eye = M[:3, 3] + R @ local_eye
                basis = self.basis @ R.T
                result = projection_bound(
                    self.plane, M, eye, basis, self.P, self.raster
                )
                self.assertTrue(result["depth_inside"])
                self.assertFalse(result["world_admission"])
                for p in [
                    self.plane.lower,
                    self.plane.upper,
                    *rng.uniform(self.plane.lower, self.plane.upper, (15, 3)),
                ]:
                    actual = exact_error(
                        self.plane, p, M, eye, basis, self.P, self.raster
                    )
                    for a, b in zip(actual, result["axis_upper_px"], strict=True):
                        self.assertLessEqual(a, F(b))
                    self.assertLessEqual(sum(actual), F(result["screen_l1_upper_px"]))

    def test_pure_origin_translation_does_not_change_encoded_relative_problem(self):
        M = np.eye(4)
        M[:3, 3] = [6378137, 800, -256]
        eye = M[:3, 3] + [25.125, -4.25, 64000.5]
        first = projection_bound(self.plane, M, eye, self.basis, self.P, self.raster)
        shift = np.array([6370000, 0, -1024])
        N = M.copy()
        N[:3, 3] -= shift
        second = projection_bound(
            self.plane, N, eye - shift, self.basis, self.P, self.raster
        )
        self.assertEqual(first, second)

    def test_tilted_support_and_jitter_or_orthographic_world_projection(self):
        M = placement(47, 8)
        eye = M[:3, 3] + M[:3, :3] @ [100, -30, 64000]
        basis = self.basis @ M[:3, :3].T
        normal = (0.2, -0.3, 1)
        matrices = (
            perspective(jitter=(0.01, -0.02)),
            np.array(
                [
                    [0.001, 0, 0.0001, 0.1],
                    [0, 0.002, 0, -0.2],
                    [0, 0, -1e-6, 1],
                    [0, 0, 0, 1],
                ],
                dtype="<f4",
            ),
        )
        for P in matrices:
            result = projection_bound(
                self.plane, M, eye, basis, P, self.raster, normal=normal
            )
            self.assertTrue(result["depth_inside"])
            for p in np.random.default_rng(1881).uniform(
                self.plane.lower, self.plane.upper, (25, 3)
            ):
                actual = exact_error(
                    self.plane, p, M, eye, basis, P, self.raster, normal
                )
                self.assertLessEqual(sum(actual), F(result["screen_l1_upper_px"]))

    def test_identity_world_frame_encloses_existing_local_bound(self):
        eye = (20, -5, 64000)
        M = np.eye(4)
        old = local_bound(self.plane, eye, self.basis, self.P, self.raster)
        new = projection_bound(self.plane, M, eye, self.basis, self.P, self.raster)
        self.assertGreaterEqual(new["screen_l1_upper_px"], old["screen_l1_upper_px"])
        self.assertLess(new["screen_l1_upper_px"] - old["screen_l1_upper_px"], 1e-10)

    def test_source_axes_are_not_renormalized_or_inverted(self):
        M = np.eye(4)
        M[0, 0] = 1 + 1e-12
        M[:3, 3] = [6378137, 1000, -1000]
        eye = M[:3, 3] + [20, -5, 64000]
        result = projection_bound(self.plane, M, eye, self.basis, self.P, self.raster)
        actual = exact_error(
            self.plane, self.plane.upper, M, eye, self.basis, self.P, self.raster
        )
        self.assertLessEqual(sum(actual), F(result["screen_l1_upper_px"]))
        matrix, _ = clip_from_field(M, eye, self.basis, self.P, self.raster)
        exact = F(float(self.P[0, 0])) * F(float(M[0, 0]))
        self.assertLessEqual(F(matrix[0][0][0]), exact)
        self.assertGreaterEqual(F(matrix[0][0][1]), exact)

    def test_float32_ecef_before_subtraction_can_wrongly_admit_budget(self):
        plane = CapturePlane((-5, -5, -0.2), (5, 5, 0.2), (0, 0, 0), (0, 0, 1))
        M = np.eye(4)
        M[:3, 3] = [0, 0, 6378137.125]
        eye = M[:3, 3] + [1.5, 0, 64.375]
        current = projection_bound(plane, M, eye, self.basis, self.P, self.raster)
        rounded = M.astype(np.float32).astype(float)
        rounded_eye = eye.astype(np.float32).astype(float)
        naive = projection_bound(
            plane, rounded, rounded_eye, self.basis, self.P, self.raster
        )
        exact = sum(
            exact_error(
                plane, plane.lower[:2] + (0.2,), M, eye, self.basis, self.P, self.raster
            )
        )
        self.assertLess(naive["screen_l1_upper_px"], float(exact))
        budget = (naive["screen_l1_upper_px"] + float(exact)) / 2
        self.assertTrue(geometric_candidate(naive, budget))
        self.assertFalse(geometric_candidate(current, budget))

    def test_passing_world_camera_directly_to_local_consumer_wrongly_admits(self):
        plane = CapturePlane((-5, -5, -0.2), (5, 5, 0.2), (0, 0, 0), (0, 0, 1))
        M = placement(0, 0)
        k = np.sqrt(0.5)
        local_basis = np.array([[1, 0, 0], [0, k, -k], [0, -k, -k]])
        eye = M[:3, 3] + M[:3, :3] @ np.array([0, 64, 64])
        basis = local_basis @ M[:3, :3].T
        wrong = local_bound(plane, eye, basis, self.P, self.raster)
        correct = projection_bound(plane, M, eye, basis, self.P, self.raster)
        actual = sum(
            exact_error(plane, plane.upper, M, eye, basis, self.P, self.raster)
        )
        self.assertTrue(geometric_candidate(wrong, 0.25))
        self.assertFalse(geometric_candidate(correct, 0.25))
        self.assertGreater(actual, F(0.25))
        self.assertLessEqual(actual, F(correct["screen_l1_upper_px"]))

    def test_absent_invalid_reflected_scaled_and_projective_frames_retain_parent(self):
        bad = [None, np.eye(3), np.full((4, 4), math.nan)]
        for i, value in ((0, -1), (0, 2)):
            M = np.eye(4)
            M[i, i] = value
            bad.append(M)
        M = np.eye(4)
        M[3, 0] = 0.01
        bad.append(M)
        for M in bad:
            result = projection_bound(
                self.plane, M, (0, 0, 64000), self.basis, self.P, self.raster
            )
            self.assertFalse(geometric_candidate(result, 0.25))
            self.assertFalse(result["world_admission"])
        missing = projection_bound(
            self.plane, None, (0, 0, 64000), self.basis, self.P, self.raster
        )
        self.assertEqual(missing["reason"], "world_from_field_unavailable")

    def test_eye_far_and_overflow_cases_do_not_become_field_candidates(self):
        M = np.eye(4)
        for eye in ((0, 0, 0), (0, 0, -2000), (math.inf, 0, 2000)):
            result = projection_bound(
                self.plane, M, eye, self.basis, self.P, self.raster
            )
            self.assertFalse(geometric_candidate(result, 0.25))
        P = np.zeros((4, 4))
        P[0, 0] = np.finfo(float).max
        P[3, 3] = 1
        result = projection_bound(
            self.plane, M, (0, 0, 64000), self.basis, P, self.raster
        )
        self.assertFalse(geometric_candidate(result, 0.25))

    def test_real_world_render_fixture_null_remains_unknown(self):
        manifest = json.loads(
            (
                ROOT
                / "test/outshine/data/render/building-world-opponents/ss2/manifest.json"
            ).read_text()
        )
        self.assertIsNone(manifest["runtime_world_from_field"])
        result = projection_bound(
            self.plane,
            manifest["runtime_world_from_field"],
            (0, 0, 64000),
            self.basis,
            self.P,
            self.raster,
        )
        self.assertFalse(geometric_candidate(result, 0.25))

    def test_archive_axes_and_origin_are_diagnostic_placement_not_runtime_identity(
        self,
    ):
        request = json.loads(
            (
                ROOT
                / "test/outshine/data/render/building-full-view/source-plane-request.json"
            ).read_text()
        )
        context = json.loads(
            (
                ROOT / "test/outshine/data/render/building-full-view/context.json"
            ).read_text()
        )
        M = np.eye(4)
        M[:3, :3] = np.array(
            [request["east"], request["up"], -np.array(request["north"])]
        ).T
        M[:3, 3] = np.array(context["scene_frame"]["origin_ecef"]) + request["origin"]
        local_eye = np.array([20, -5, 64000])
        eye = M[:3, 3] + M[:3, :3] @ local_eye
        basis = self.basis @ M[:3, :3].T
        result = projection_bound(self.plane, M, eye, basis, self.P, self.raster)
        actual = sum(
            exact_error(
                self.plane, self.plane.upper, M, eye, basis, self.P, self.raster
            )
        )
        self.assertLessEqual(actual, F(result["screen_l1_upper_px"]))
        self.assertFalse(result["world_admission"])


if __name__ == "__main__":
    unittest.main()
