"""Independent finite-sample controls of the new host-field adapters."""

import unittest

import numpy as np
from crown_phase_probe import mesh_errors
from raster_batch import rasterize
from spatial_normal_fields import encode


class SpatialFields(unittest.TestCase):
    def test_visible_normal_response_matches_scalar_source_samples(self):
        frame = np.array(((0, -1, 0), (1, 0, 0), (0, 0, 1)), float)
        corners = np.array(((0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)), float)
        source = {
            "vertices": corners,
            "faces": np.array(((0, 1, 2), (0, 2, 3))),
            "normals": np.repeat(np.array(((0.6, 0, 0.8), (-0.6, 0, 0.8))), 3, axis=0),
        }
        lights = np.array(((1, 0, 0), (-1, 0, 0), (0, 0, 1), (0, 0, -1)), float)
        measure, _, _ = encode(source, frame, corners, lights, size=4)
        p = corners @ frame
        mask, normals = rasterize(
            p,
            source["faces"],
            source["normals"].reshape(-1, 3, 3),
            p[:, :2].min(axis=0),
            p[:, :2].max(axis=0),
            8,
        )
        exact = np.zeros((4, 16))
        for y, x in zip(*np.nonzero(mask), strict=True):
            n = np.array(normals[y, x], dtype=float)
            n /= np.linalg.norm(n)
            for i, l in enumerate(lights):
                exact[i, y // 2 * 4 + x // 2] += 0.25 * max(0, float(n @ l))
        np.testing.assert_allclose(measure.exact, exact, rtol=0, atol=1e-15)
        np.testing.assert_array_equal(
            measure.moments[24][..., 0].sum(axis=1), np.ones(16)
        )
        self.assertGreater(exact[:2].sum(), 0)
        self.assertEqual(float(exact[3].sum()), 0)

    def test_corresponding_triangle_displacement_and_normal_error(self):
        points = np.array(((0, 0, 0), (1, 0, 0), (0, 1, 0)), float)
        normals = np.array(((0, 0, 1),) * 3, float)
        mesh = {"vertices": points, "normals": normals}
        actual = {
            "variants": np.array((0,)),
            "origins": np.array(((0, 0, 0),)),
            "frames": np.eye(3)[None],
        }
        proxy = actual | {"origins": np.array(((0, 0, 2),))}
        distance, normal = mesh_errors(actual, proxy, [mesh])
        self.assertEqual(distance, 2.0)
        self.assertEqual(normal, 0.0)
        # Rotate a planar triangle without changing its normal: points still move.
        proxy = actual | {"frames": np.diag((-1, -1, 1))[None]}
        distance, normal = mesh_errors(actual, proxy, [mesh])
        self.assertEqual(distance, 2.0)
        self.assertEqual(normal, 0.0)


if __name__ == "__main__":
    unittest.main()
