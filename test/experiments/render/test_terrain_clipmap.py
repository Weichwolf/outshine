"""Independent topology and morph-continuity checks for #515."""

import unittest

import numpy as np

from terrain_clipmap import layout, positions, triangle_count


def hills(xy):
    return 100 * np.sin(xy[:, 0] / 11) * np.cos(xy[:, 1] / 17)


class ClipmapTopology(unittest.TestCase):
    def test_all_snap_phases_are_watertight_without_skirts(self):
        for camera in [(0, 0), (1.9, 3.9), (-3.9, 7.9), (31.5, -17.25)]:
            mesh = layout(camera, levels=4, half_cells=8)
            points = mesh.xy[mesh.triangles]
            a, b = points[:, 1] - points[:, 0], points[:, 2] - points[:, 0]
            area2 = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
            self.assertTrue(np.all(area2 > 0))
            self.assertEqual(len(points), triangle_count(4, 8))
            self.assertAlmostEqual(area2.sum() / 2, np.prod(mesh.outer_bounds[1] - mesh.outer_bounds[0]))
            edges = mesh.triangles[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
            unique, count = np.unique(np.sort(edges, axis=1), axis=0, return_counts=True)
            self.assertTrue(np.all((count == 1) | (count == 2)))
            boundary = mesh.xy[unique[count == 1]]
            on_outer_edge = np.zeros(len(boundary), dtype=bool)
            for axis in range(2):
                for edge in mesh.outer_bounds[:, axis]:
                    on_outer_edge |= np.all(boundary[:, :, axis] == edge, axis=1)
            self.assertTrue(np.all(on_outer_edge), "every unmatched edge must belong to the outer boundary")

    def test_morph_does_not_jump_at_camera_snap(self):
        for snap in [-16, -8, -4, -2, 2, 4, 8, 16]:
            for y in [0, snap]:
                before = layout((snap - 1e-7, y), levels=4, half_cells=8)
                after = layout((snap + 1e-7, y), levels=4, half_cells=8)
                a, b = positions(before, hills), positions(after, hills)
                common = {tuple(p[:2]): p[2] for p in a}
                delta = [abs(p[2] - common[tuple(p[:2])]) for p in b if tuple(p[:2]) in common]
                self.assertLess(max(delta), 1e-4, (snap, y))

    def test_affine_height_is_preserved(self):
        mesh = layout((7.1, -9.8), levels=5, half_cells=8)
        sample = lambda xy: 0.25 * xy[:, 0] - 0.5 * xy[:, 1] + 17
        np.testing.assert_allclose(positions(mesh, sample)[:, 2], sample(mesh.xy), rtol=0, atol=1e-12)

    def test_missing_height_data_is_not_replaced_by_flat_ground(self):
        mesh = layout(levels=2, half_cells=8)
        with self.assertRaises(ValueError):
            positions(mesh, lambda xy: np.full(len(xy), np.nan))

    def test_grid_requires_room_for_both_inner_ring_and_morph_band(self):
        with self.assertRaises(ValueError):
            layout(half_cells=4)

    def test_fine_near_grid_can_extend_to_the_far_world(self):
        mesh = layout(levels=18, half_cells=8, cell_m=0.25)
        with np.errstate(over="raise", invalid="raise"):
            result = positions(mesh, hills)
        self.assertTrue(np.all(np.isfinite(result)))
        self.assertGreater(mesh.outer_bounds[1, 0], 240000)


if __name__ == "__main__":
    unittest.main()
