"""Whole hard terrain references survive without obsolete soft interior constraints."""

import unittest
from pathlib import Path

import numpy as np
import shapely
from stair_free_terrain import retained_seeds
from stair_module_soil import tessellate
from stair_module_topology import check_surface


class FreeTerrainTriangulation(unittest.TestCase):
    def test_free_center_is_removed_but_old_boundary_and_hard_center_are_retained(self):
        xy = np.array([[x, y] for x in (0, 5, 10) for y in (0, 5, 10)], float)
        seeds = np.arange(len(xy))
        fixed = np.array([[[3, 3], [7, 3], [7, 7]], [[3, 3], [7, 7], [3, 7]]], float)
        region = shapely.box(0, 0, 10, 10)
        kept, note = retained_seeds(xy, seeds, fixed, region)
        self.assertNotIn(4, kept)
        self.assertEqual(len(kept), 8)
        self.assertEqual(note["omitted_free_interior_vertices"], 1)
        hard = np.concatenate([fixed, np.array([[[5, 5], [6, 5], [6, 6]]])])
        hard_kept, _note = retained_seeds(xy, seeds, hard, region)
        np.testing.assert_array_equal(hard_kept, seeds)

    def test_real_whole_case_keeps_coverage_and_hard_vertices_under_unchanged_Q_gate(
        self,
    ):
        path = (
            Path(__file__).parents[2]
            / "outshine/data/transport/free-terrain-planar-case.npz"
        )
        with np.load(path, allow_pickle=False) as data:
            xy, faces, fixed = (
                data[k] for k in ("old_xy", "selected_faces", "fixed_triangles_xy_m")
            )
        before = xy.copy()
        seeds = np.unique(faces)
        old_shapes = shapely.polygons(xy[faces])
        region = shapely.union_all(old_shapes)
        ground = shapely.union_all(shapely.polygons(fixed))
        legacy_free = shapely.get_parts(shapely.difference(old_shapes, ground))
        old_xy, old_faces, _work = tessellate(xy, seeds, fixed, legacy_free)
        with self.assertRaisesRegex(ValueError, "collapses or inverts"):
            check_surface(old_xy, old_faces)
        kept, note = retained_seeds(xy, seeds, fixed, region)
        free = shapely.get_parts(region.difference(ground))
        actual, triangles, _work = tessellate(xy, kept, fixed, free)
        check_surface(actual, triangles)
        np.testing.assert_array_equal(xy, before)
        self.assertLess(len(actual), len(old_xy))
        self.assertLess(len(triangles), len(old_faces))
        self.assertEqual(note["omitted_free_interior_vertices"], 101)
        points = {tuple(point) for point in actual}
        self.assertTrue(all(tuple(point) in points for point in xy[kept]))
        self.assertTrue(all(tuple(point) in points for point in fixed.reshape(-1, 2)))
        shown = shapely.union_all(shapely.polygons(actual[triangles]))
        self.assertLessEqual(region.symmetric_difference(shown).area, 1e-7)


if __name__ == "__main__":
    unittest.main()
