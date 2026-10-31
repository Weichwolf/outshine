import unittest

import numpy as np
import shapely
from shore_bank_strip import build


class ShoreBankStripTests(unittest.TestCase):
    def test_independent_interface_nodes_are_preserved_and_body_closes(self):
        shore = np.array([[0, 0, 10], [2, 0, 10.1], [5, 0, 10.3]])
        road = np.array([[0, 1, 16], [1, 1, 16.2], [4, 2, 16.4], [5, 2, 16.5]])
        rim = np.array([15.0, 15.2, 15.4])
        body = build(shore, rim, road, bed_depth_m=2)
        np.testing.assert_array_equal(body["vertices"][body["road_upper"]], road)
        np.testing.assert_array_equal(body["vertices"][body["shore_upper"], 2], rim)
        np.testing.assert_array_equal(
            body["vertices"][body["shore_lower"]], shore - [0, 0, 2]
        )
        f = body["faces"]
        _, count = np.unique(
            np.sort(f[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1),
            axis=0,
            return_counts=True,
        )
        self.assertTrue(np.all(count == 2))
        roof = body["vertices"][body["roof_faces"], :2]
        self.assertTrue(shapely.coverage_is_valid(shapely.polygons(roof)))
        self.assertEqual(body["continuation_edges"].shape, (2, 2))

    def test_other_side_has_consistent_winding(self):
        shore = np.array([[0, 0, 10], [2, 0, 10.1]])
        road = np.array([[0, -1, 16], [2, -1, 16.2]])
        body = build(shore, [15, 15.2], road, bed_depth_m=2)
        self.assertEqual(len(body["faces"]), 12)

    def test_crossed_inverted_and_missing_interfaces_decline(self):
        shore = [[0, 0, 10], [2, 0, 10]]
        for road, rim in (
            ([[2, 1, 16], [0, 1, 16]], [15, 15]),
            ([[0, 1, 16], [2, 1, 16]], [7, 15]),
            ([[0, 1, 16]], [15, 15]),
        ):
            with self.assertRaises(ValueError):
                build(shore, rim, road, bed_depth_m=2)


if __name__ == "__main__":
    unittest.main()
