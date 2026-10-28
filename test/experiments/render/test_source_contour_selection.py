import unittest

import numpy as np
import shapely
from source_contour_selection import project_contours, select_contours
from source_surface_patches import SurfacePatch


def plane(height, domain=None, slope=0):
    axes = np.array([[1, 0, slope], [0, 1, 0]], float)
    normal = np.cross(*axes)
    normal /= np.linalg.norm(normal)
    return SurfacePatch(
        shapely.box(-1, -1, 1, 1) if domain is None else domain,
        np.array([0, 0, height], float),
        axes,
        normal,
        0,
    )


def projection(patches):
    return project_contours(patches, [0, 0, 0], [0, 0, 1], [1, 0, 0], [0, 1, 0])


class SourceContourSelection(unittest.TestCase):
    def test_opaque_front_contour_excludes_hidden_rear(self):
        selected, owners, _ = select_contours(projection([plane(0), plane(1)]), 16, 1)
        np.testing.assert_array_equal(selected, [1])
        self.assertTrue(np.all(owners == 1))

    def test_hole_exposes_rear_contour(self):
        ring = shapely.Polygon(
            [(-1, -1), (1, -1), (1, 1), (-1, 1)],
            holes=[[(-0.5, -0.5), (-0.5, 0.5), (0.5, 0.5), (0.5, -0.5)]],
        )
        selected, owners, _ = select_contours(
            projection([plane(0), plane(1, ring)]), 16, 1
        )
        np.testing.assert_array_equal(selected, [0, 1])
        self.assertEqual(owners[8, 8], 0)
        self.assertEqual(owners[0, 0], 1)

    def test_crossing_planes_use_depth_at_each_ray(self):
        selected, owners, _ = select_contours(
            projection([plane(0, slope=1), plane(0, slope=-1)]), 16, 1
        )
        np.testing.assert_array_equal(selected, [0, 1])
        self.assertTrue(np.all(owners[:, :8] == 1))
        self.assertTrue(np.all(owners[:, 8:] == 0))

    def test_repeated_coplanar_contours_have_stable_ties(self):
        selected, _, _ = select_contours(projection([plane(0), plane(0)]), 16, 1)
        np.testing.assert_array_equal(selected, [0])

    def test_subpixel_contour_is_explicitly_not_covered_by_the_sampler(self):
        selected, _, _ = select_contours(
            projection([plane(0, shapely.box(0.01, -1, 0.02, 1))]), 16, 1
        )
        self.assertEqual(len(selected), 0)

    def test_backfaces_do_not_occlude(self):
        back = plane(1)
        back.normal *= -1
        selected, _, _ = select_contours(projection([plane(0), back]), 16, 1)
        np.testing.assert_array_equal(selected, [0])

    def test_projection_rounding_collapse_is_reported_as_unresolved(self):
        narrow = plane(0, shapely.box(0, -1, 1e-16, 1))
        projected = project_contours(
            [narrow], [-1000, 0, 0], [0, 0, 1], [1, 0, 0], [0, 1, 0]
        )
        np.testing.assert_array_equal(projected.collapsed, [0])
        selected, _, _ = select_contours(projected, 16, 1)
        self.assertEqual(len(selected), 0)

    def test_pair_budget_exhaustion_returns_no_partial_selection(self):
        with self.assertRaisesRegex(ValueError, "budget exhausted"):
            select_contours(projection([plane(0)]), 16, 1, maximum_pairs=0)

    def test_invalid_frame_and_raster_are_rejected(self):
        with self.assertRaises(ValueError):
            project_contours([plane(0)], [0, 0, 0], [0, 0, 2], [1, 0, 0], [0, 1, 0])
        for pixels in (0, 257, True):
            with self.assertRaises(ValueError):
                select_contours(projection([plane(0)]), pixels, 1)


if __name__ == "__main__":
    unittest.main()
