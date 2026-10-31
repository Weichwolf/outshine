"""Interior, hole, hidden layer and front-face counterexamples for planar queries."""

import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from source_domain_ray_reference import intersect
from source_surface_patches import SurfacePatch


def patch(domain, height, orientation=1):
    return SimpleNamespace(
        geometry=SurfacePatch(
            domain,
            np.array([0, 0, height]),
            np.eye(3)[:2],
            np.array([0, 0, orientation]),
            1,
        )
    )


class DomainRayReference(unittest.TestCase):
    def test_hole_keeps_lower_layer_and_backfaces_do_not_hide_it(self):
        ring = shapely.Polygon(
            [(0, 0), (4, 0), (4, 4), (0, 4)], [[(1, 1), (1, 3), (3, 3), (3, 1)]]
        )
        patches = [
            patch(shapely.box(0, 0, 4, 4), 3, -1),
            patch(ring, 2),
            patch(shapely.box(0, 0, 4, 4), 0),
        ]
        origins = np.array([[0.5, 0.5, 4], [2, 2, 4], [5, 2, 4]])
        directions = np.tile([0, 0, -1], (3, 1))
        depth, owner, _ = intersect(patches, origins, directions)
        np.testing.assert_array_equal(owner, [1, 2, -1])
        np.testing.assert_array_equal(depth[:2], [2, 4])
        self.assertTrue(np.isinf(depth[2]))

    def test_degenerate_rays_and_work_overflow_do_not_produce_empty_claims(self):
        p = [patch(shapely.box(0, 0, 1, 1), 0)]
        with self.assertRaises(ValueError):
            intersect(p, [[0, 0, 1]], [[0, 0, 0]])
        with self.assertRaises(ValueError):
            intersect(p, [[0, 0, 1]], [[0, 0, -1]], pairs_most=0)


if __name__ == "__main__":
    unittest.main()
