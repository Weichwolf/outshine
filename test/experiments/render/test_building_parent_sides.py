"""Side descriptors keep grazing/unknown faces and pay more than tiny boxes save."""

import unittest

import numpy as np
import shapely
from building_mass_mesh import extrude
from building_parent_sides import prepare_sides, query_cones, query_sides
from facing_cones import cone_excludes, prepare_cone
from native_backface_cost import unit_normals


class BuildingParentSides(unittest.TestCase):
    def test_prepared_cone_keeps_grazing_and_front_views_without_geometry(self):
        points = np.array([[[-1, -1, 0], [1, -1, 0], [0, 1, 0.0]]])
        cone = prepare_cone(points, unit_normals(points))
        self.assertTrue(cone_excludes(cone, np.array([0, 0, -3]), 1e-6))
        for eye in ([0, 0, 3], [1, 1, 0]):
            self.assertFalse(cone_excludes(cone, np.array(eye), 1e-6))
        rejected, _ = query_cones(cone[None], np.array([0, 0, -3]))
        np.testing.assert_array_equal(rejected, [True])
        self.assertFalse(cone_excludes(cone, np.array([0, 0, -3]), 4))
        self.assertFalse(cone_excludes(None, np.zeros(3), 1e-6))

    def test_closed_box_partition_and_metadata_reverse_apparent_index_gain(self):
        mesh, _ = extrude(shapely.box(0, 0, 1, 1), [0, 1])
        data = {"g__" + k: v for k, v in mesh.items()} | {
            "g__triangle_groups": np.zeros(len(mesh["indices"]), np.uint16)
        }
        ready = prepare_sides(data)
        np.testing.assert_array_equal(
            np.sort(np.concatenate(ready["members"])), np.arange(len(mesh["indices"]))
        )
        kept, work = query_sides(ready, data, np.array([4, 3, 5]))
        self.assertEqual(work["whole_parent_cone_rejected_triangles"], 0)
        self.assertEqual(len(kept), 6)
        self.assertEqual(work["descriptor_and_range_bytes"], 6 * 72)
        self.assertGreater(
            work["selected_render_array_bytes_with_metadata"],
            work["baseline_render_array_bytes"],
        )
        self.assertEqual(work["unchanged_positions_bytes"], data["g__positions"].nbytes)

    def test_degenerate_unknown_triangle_fails_open(self):
        data = {
            "g__positions": np.zeros((3, 3), np.float32),
            "g__indices": np.array([[0, 1, 2]], np.uint16),
            "g__triangle_groups": np.zeros(1, np.uint16),
        }
        kept, cost = query_sides(prepare_sides(data), data, np.array([0, 0, -10]))
        np.testing.assert_array_equal(kept, [0])
        self.assertEqual(cost["rejected_triangles"], 0)


if __name__ == "__main__":
    unittest.main()
