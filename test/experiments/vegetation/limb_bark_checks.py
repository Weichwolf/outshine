"""Independent metric, topology, ownership and preflight controls for C1 bark."""

import unittest
from itertools import pairwise
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from limb_bark import station_distances
from limb_experiment import canonical_wood
from limb_surface import limb_mesh, path_recipe
from scipy.integrate import quad
from scipy.spatial.transform import Rotation


class LimbBarkChecks(unittest.TestCase):
    def setUp(self):
        t = np.linspace(0, 1, 9)
        self.points = np.column_stack((0.4 * np.sin(t), 0.2 * t**2, 3 * t))
        self.radii = np.linspace(0.07, 0.012, len(t))
        self.path = np.arange(len(t))

    def test_distances_match_independent_adaptive_integrals(self):
        for count in (2, 9, 68):
            t = np.linspace(0, 1, count)
            points = np.column_stack((0.4 * np.sin(t), 0.2 * t**2, 3 * t))
            spine, _ = path_recipe(points, np.full(count, 0.02), np.arange(count))
            parameters = np.linspace(0, 1, (count - 1) * 2 + 1)
            expected = [0]
            for a, b in pairwise(parameters):
                length = quad(
                    lambda u, curve=spine: np.linalg.norm(curve.evaluate(u, 1)),
                    a,
                    b,
                    epsabs=1e-12,
                    epsrel=1e-12,
                )[0]
                expected.append(expected[-1] + length)
            np.testing.assert_allclose(
                station_distances(spine, parameters), expected, rtol=2e-12, atol=2e-12
            )

    def test_straight_axis_analytic_scale_and_cap_centres(self):
        for scale in (0.1, 1, 10):
            mesh = limb_mesh(
                np.array(((0, 0, 0), (0, 0, 3))) * scale,
                np.array((0.1, 0.04)) * scale,
                (0, 1),
                bark=True,
            )
            chart = mesh["bark_coordinates"]
            np.testing.assert_allclose(chart[-2:], ((0, 0, 0), (0, 0, 3 * scale)))
            rings = chart[:-2].reshape(3, 8, 3)
            np.testing.assert_allclose(rings[:, 0, 2], np.linspace(0, 3 * scale, 3))
            np.testing.assert_allclose(
                np.linalg.norm(rings[:, :, :2], axis=2),
                mesh["bark_radius"][:-2].reshape(3, 8),
            )

    def test_same_geometry_and_cyclic_shared_vertices(self):
        old = limb_mesh(self.points, self.radii, self.path)
        new = limb_mesh(self.points, self.radii, self.path, bark=True)
        for key in old:
            np.testing.assert_array_equal(old[key], new[key])
        # The angular wrap uses the original vertex; no duplicated U=0/U=1 seam.
        side_faces = new["faces"][:-16].reshape(-1, 8, 2, 3)
        for station, ring in enumerate(side_faces):
            self.assertIn(station * 8, ring[-1].ravel())
            self.assertIn(station * 8, ring[0].ravel())
        self.assertTrue(np.isfinite(new["bark_coordinates"]).all())

    def test_rigid_transform_and_uniform_physical_scaling(self):
        original = limb_mesh(self.points, self.radii, self.path, bark=True)
        rotation = Rotation.from_rotvec((0.3, -0.4, 0.5)).as_matrix()
        for scale in (0.2, 4):
            changed = limb_mesh(
                self.points @ rotation.T * scale + (2, -3, 1),
                self.radii * scale,
                self.path,
                bark=True,
            )
            for key in ("bark_coordinates", "bark_radius"):
                np.testing.assert_allclose(
                    changed[key], original[key] * scale, rtol=1e-11, atol=1e-12
                )

    def test_branched_concatenation_preserves_owners_and_all_arrays(self):
        points = np.array(((0, 0, 0), (0, 0, 1), (0.3, 0, 2), (-0.5, 0, 1.8)))
        crown = SimpleNamespace(positions=points, parents=np.array((-1, 0, 1, 1)))
        old, paths = canonical_wood(crown)
        new, again = canonical_wood(crown, bark=True)
        self.assertEqual(paths, again)
        for key in old:
            np.testing.assert_array_equal(old[key], new[key])
        offset = 0
        for path in paths:
            n = ((len(path) - 1) * 2 + 1) * 8 + 2
            self.assertEqual(new["bark_coordinates"][offset, 2], 0)
            offset += n
        self.assertEqual(offset, len(new["vertices"]))

    def test_capacity_precedes_chart_work_and_flag_is_explicit(self):
        with patch("limb_bark.bark_fields", side_effect=AssertionError):
            self.assertIsNone(
                limb_mesh(self.points, self.radii, self.path, bark=True, max_faces=0)
            )
            limb_mesh(self.points, self.radii, self.path)
        for flag in (1, None, "yes"):
            with self.assertRaises(ValueError):
                limb_mesh(self.points, self.radii, self.path, bark=flag)


if __name__ == "__main__":
    unittest.main()
