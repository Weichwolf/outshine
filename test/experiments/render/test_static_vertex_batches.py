import unittest

import numpy as np

from static_vertex_batches import Draw, batch


class StaticVertexBatchTests(unittest.TestCase):
    def test_permuted_vertices_and_placements_keep_original_indices(self):
        draws = [Draw(3, 3, 7, 0), Draw(0, 3, 2, 0), Draw(6, 3, 1, 1)]
        models, groups, fallback = batch(9, draws)
        self.assertEqual(groups, {0: [1, 0], 1: [2]})
        self.assertEqual(fallback, [])
        indices = np.array([5, 3, 4, 2, 0, 1, 8, 6, 7])
        np.testing.assert_array_equal(models[indices], [7, 7, 7, 2, 2, 2, 1, 1, 1])
        self.assertEqual(models.nbytes, 9 * 4)

    def test_shared_geometry_with_different_models_retains_original_draws(self):
        models, groups, fallback = batch(6, [Draw(0, 3, 0, 0), Draw(0, 3, 1, 0),
                                            Draw(3, 3, 2, 0)])
        self.assertEqual(groups, {0: [2]})
        self.assertEqual(fallback, [0, 1])
        self.assertTrue(np.all(models[:3] == np.iinfo(np.uint32).max))

    def test_instanced_meshes_do_not_expand_vertices(self):
        models, groups, fallback = batch(3, [Draw(0, 3, 5, 0, instances=1000000)])
        self.assertEqual(groups, {})
        self.assertEqual(fallback, [0])
        self.assertEqual(models.nbytes, 12)

    def test_material_groups_can_share_one_placement_without_vertex_duplication(self):
        models, groups, fallback = batch(3, [Draw(0, 3, 5, 0), Draw(0, 3, 5, 1)])
        self.assertEqual(groups, {0: [0], 1: [1]})
        self.assertEqual(fallback, [])
        np.testing.assert_array_equal(models, [5, 5, 5])

    def test_invalid_or_unsupported_ranges_fail_explicitly(self):
        for draws in ([Draw(0, 3, 0, 0), Draw(1, 3, 1, 0)],
                      [Draw(0, 5, 0, 0)], [Draw(0, 3, -1, 0)],
                      [Draw(0, 3, 0, 0, instances=0)]):
            with self.assertRaises(ValueError):
                batch(4, draws)


if __name__ == "__main__":
    unittest.main()
