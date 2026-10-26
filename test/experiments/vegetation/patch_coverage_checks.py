"""Independent alpha-union control and equivalence to the old full-shading probe."""

import unittest

import numpy as np
from cloud_experiment import view
from group_cloud import Chart
from patch_coverage import coverage


class PatchCoverage(unittest.TestCase):
    def test_depth_order_and_quad_diagonal_do_not_change_alpha_union(self):
        vertices = np.array(((0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)), float)
        faces = np.array(((0, 1, 2), (0, 2, 3)), np.int32)
        source = {
            "vertices": vertices,
            "faces": faces,
            "normals": np.tile((0.0, 0.0, 1.0), (6, 1)),
        }
        mesh = {
            "vertices": vertices,
            "faces": faces,
            "normals": source["normals"],
            "uv": vertices[:, :2],
        }
        normals = np.broadcast_to((0.25, 0.25, 0.5, 0.5), (16, 16, 4)).copy()
        chart = Chart(mesh, normals, normals, np.array((0,)), np.eye(3), 0.0, 0.0)
        behind = Chart(
            mesh | {"vertices": vertices + np.array((0, 0, -0.1))},
            normals,
            normals,
            np.array((1,)),
            np.eye(3),
            -0.1,
            0.0,
        )
        bounds = (np.zeros(2), np.ones(2))
        # Existing view uses a different image plane orientation; both use same bounds.
        direction = (0, 1, 0)
        # Put the square in that image plane so every raster sample is covered.
        rotate = np.array(((1, 0, 0), (0, 0, -1), (0, 1, 0)), float)
        source = source | {
            "vertices": vertices @ rotate.T,
            "normals": source["normals"] @ rotate.T,
        }
        chart = Chart(
            mesh | {"vertices": vertices @ rotate.T},
            normals,
            normals,
            np.array((0,)),
            rotate,
            0.0,
            0.0,
        )
        behind = Chart(
            mesh | {"vertices": (vertices + np.array((0, 0, -0.1))) @ rotate.T},
            normals,
            normals,
            np.array((1,)),
            rotate,
            -0.1,
            0.0,
        )
        a, ref, red = coverage(source, (chart, behind), direction, bounds)
        b, _, reverse = coverage(source, (behind, chart), direction, bounds)
        old, oldref, oldred = view(
            source, (chart, behind), direction, bounds, fields=True
        )
        np.testing.assert_array_equal(ref, oldref)
        np.testing.assert_allclose(red, oldred, rtol=0, atol=2e-15)
        np.testing.assert_array_equal(reverse, red)
        np.testing.assert_allclose(red, 1 - (1 - 128 / 255) ** 2, rtol=0, atol=2e-15)
        self.assertEqual(a, b)
        self.assertAlmostEqual(
            a["coverage_l1_over_union"], old["coverage_l1_over_union"], 14
        )


if __name__ == "__main__":
    unittest.main()
