"""Global Hermite reference, piece-boundary skin topology and preflight controls."""

import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from limb_experiment import canonical_wood
from limb_surface import limb_mesh
from long_limb_surface import piece_samples, piecewise_limb_mesh
from outshine_ref.vegetation import CubicSpine
from scipy.interpolate import CubicHermiteSpline


def curve_source(count=91):
    x = np.linspace(0, 18, count)
    points = np.c_[x, 0.12 * np.sin(0.12 * x), 0.02 * x * x]
    return points, np.linspace(0.04, 0.01, count), tuple(range(count))


def scalar_derivatives(points):
    result = []
    for i in range(len(points)):
        if i == 0:
            value = points[1] - points[0]
        elif i == len(points) - 1:
            value = points[-1] - points[-2]
        else:
            a, b = points[i] - points[i - 1], points[i + 1] - points[i]
            direction = a / np.linalg.norm(a) + b / np.linalg.norm(b)
            value = (
                direction
                / np.linalg.norm(direction)
                * min(np.linalg.norm(a), np.linalg.norm(b))
            )
        result.append(value)
    return np.array(result)


class LongLimbTests(unittest.TestCase):
    def test_bounded_pieces_equal_one_independent_global_cubic_interpolator(self):
        points, radii, path = curve_source()
        derivative = scalar_derivatives(points) * (len(points) - 1)
        oracle = CubicHermiteSpline(np.linspace(0, 1, len(points)), points, derivative)
        u = np.linspace(0, 1, 2 * (len(points) - 1) + 1)
        v, a = oracle(u, 1), oracle(u, 2)
        speed = np.linalg.norm(v, axis=1)
        tangent = v / speed[:, None]
        curvature = (a - np.sum(a * tangent, axis=1)[:, None] * tangent) / speed[
            :, None
        ] ** 2
        for chunks in (1, 7, 32, 64):
            c, t, s, k, _ = piece_samples(points, radii, path, 2, chunks)
            np.testing.assert_allclose(c, oracle(u), atol=3e-13, rtol=0)
            np.testing.assert_allclose(c[::2], points, atol=3e-13, rtol=0)
            np.testing.assert_allclose(t, tangent, atol=3e-12, rtol=0)
            np.testing.assert_allclose(s, speed, atol=3e-11, rtol=0)
            np.testing.assert_allclose(k, curvature, atol=3e-11, rtol=0)

    def test_long_straight_skin_has_only_two_caps_and_independent_frustum_volume(self):
        points = np.c_[np.zeros((91, 2)), np.linspace(0, 9, 91)]
        radii = np.linspace(0.04, 0.01, 91)
        path = tuple(range(91))
        mesh = piecewise_limb_mesh(points, radii, path)
        vertices, faces = mesh["vertices"], mesh["faces"]
        self.assertEqual(len(faces), 16 * 181)
        cap = faces.max(axis=1) >= 181 * 8
        self.assertEqual(int(cap.sum()), 16)
        cut = set(range(128 * 8, 129 * 8))
        self.assertFalse(any(all(int(i) in cut for i in f) for f in faces))
        p = vertices[faces]
        actual = np.einsum("ij,ij->i", p[:, 0], np.cross(p[:, 1], p[:, 2])).sum() / 6
        centres = vertices[:-2].reshape(181, 8, 3).mean(axis=1)
        radius = np.linalg.norm(
            vertices[:-2].reshape(181, 8, 3)[:, 0] - centres, axis=1
        )
        expected = (
            8
            * np.sin(2 * np.pi / 8)
            / 6
            * np.sum(
                np.diff(centres[:, 2])
                * (radius[:-1] ** 2 + radius[:-1] * radius[1:] + radius[1:] ** 2)
            )
        )
        self.assertAlmostEqual(actual, expected, places=13)
        edges = np.concatenate([faces[:, i] for i in ([0, 1], [1, 2], [2, 0])])
        unique, count = np.unique(np.sort(edges, axis=1), axis=0, return_counts=True)
        self.assertTrue((count == 2).all())
        self.assertEqual(len(vertices) - len(unique) + len(faces), 2)
        np.testing.assert_allclose(
            np.linalg.norm(mesh["normals"], axis=1), 1, atol=2e-14
        )

    def test_short_paths_keep_old_mesh_bytes(self):
        points, radii, path = curve_source(33)
        a = limb_mesh(points, radii, path)
        b = piecewise_limb_mesh(points, radii, path)
        for key in a:
            np.testing.assert_array_equal(a[key], b[key])

    def test_frame_and_geometry_capacity_precede_piece_sampling(self):
        points, radii, path = curve_source()
        with patch(
            "long_limb_surface.piece_samples", side_effect=AssertionError("sampling")
        ):
            self.assertIsNone(piecewise_limb_mesh(points, radii, path, max_faces=0))
            self.assertIsNone(piecewise_limb_mesh(points, radii, path, max_vertices=0))
            self.assertIsNone(piecewise_limb_mesh(*curve_source(600)))
        with self.assertRaises(ValueError):
            piecewise_limb_mesh(points, radii, path, along=0)
        with self.assertRaises(ValueError):
            piecewise_limb_mesh(points, radii, path, chunk_edges=0)

    def test_existing_cubic_and_single_piece_limits_are_unchanged(self):
        points, radii, path = curve_source()
        with self.assertRaises(ValueError):
            limb_mesh(points, radii, path)
        with self.assertRaises(ValueError):
            CubicSpine.from_hermite(points, scalar_derivatives(points) * 90)

    def test_whole_request_capacity_precedes_all_curve_work(self):
        points, radii, _path = curve_source()
        crown = SimpleNamespace(
            positions=points, parents=np.arange(-1, len(points) - 1, dtype=np.int32)
        )
        with patch(
            "limb_experiment.piecewise_limb_mesh",
            side_effect=AssertionError("geometry"),
        ):
            self.assertIsNone(canonical_wood(crown, radii=radii, max_faces=1))
            self.assertIsNone(canonical_wood(crown, radii=radii, max_nodes=32))


if __name__ == "__main__":
    unittest.main()
