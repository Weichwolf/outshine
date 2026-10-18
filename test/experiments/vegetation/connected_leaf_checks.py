"""Independent connected topology/root and triangle-pair bounds controls."""

import unittest

import numpy as np
from connected_leaf import build_connected, paired_surface_bound
from outshine_ref.vegetation import Blade


class ConnectedLeaf(unittest.TestCase):
    def test_equal_total_area_cannot_hide_a_gap_and_duplicated_half(self):
        xy = np.array(((0, 0), (1, 0), (1, 1), (0, 1)), float)
        source = {
            "vertices": np.column_stack((xy, np.zeros(4))),
            "faces": np.array(((0, 1, 2), (0, 2, 3)), np.int32),
        }
        half = xy * np.array((0.5, 1.0))
        carrier = {
            "vertices": np.column_stack((half, np.zeros(4))),
            "faces": np.tile(source["faces"], (2, 1)),
        }
        with self.assertRaisesRegex(ValueError, "uncovered"):
            paired_surface_bound(source, xy, carrier, half)

    def test_carrier_has_shared_internal_edges_and_exact_attachment(self):
        blade = Blade(length=0.2, width=0.06, bend=0.4, twist=0.2)
        p = build_connected(blade, size=32)
        mesh = p["mesh"]
        edges = {}
        for face in mesh["faces"]:
            for a, b in zip(face, np.roll(face, -1), strict=True):
                key = tuple(sorted((int(a), int(b))))
                edges[key] = edges.get(key, 0) + 1
        self.assertEqual(len(mesh["vertices"]), 9)
        self.assertEqual(len(mesh["faces"]), 8)
        self.assertEqual(sum(n == 2 for n in edges.values()), 8)
        self.assertEqual(sum(n == 1 for n in edges.values()), 8)
        np.testing.assert_array_equal(mesh["vertices"][1], (0, -blade.petiole, 0))
        np.testing.assert_array_equal(p["colour"][..., 3], p["normal"][..., 3])
        self.assertLess(p["paired_source_surface_bound_m"], 0.02)

    def test_plane_translation_pair_bound_is_exact(self):
        xy = np.array(((0, 0), (1, 0), (1, 1), (0, 1)), float)
        vertices = np.column_stack((xy, np.zeros(4)))
        source = {
            "vertices": vertices,
            "faces": np.array(((0, 1, 2), (0, 2, 3)), np.int32),
        }
        # Opposite diagonals require intersection vertices, not only original corners.
        carrier = {
            "vertices": vertices + np.array((0, 0, 0.3)),
            "faces": np.array(((0, 1, 3), (1, 2, 3)), np.int32),
        }
        self.assertAlmostEqual(paired_surface_bound(source, xy, carrier, xy), 0.3, 14)

    def test_diagonal_crossing_detects_missing_interior_vertex_bound(self):
        xy = np.array(((0, 0), (1, 0), (1, 1), (0, 1)), float)
        v = np.column_stack((xy, np.array((0.0, 0.0, 1.0, 0.0))))
        source = {"vertices": v, "faces": np.array(((0, 1, 2), (0, 2, 3)), np.int32)}
        carrier = {"vertices": v, "faces": np.array(((0, 1, 3), (1, 2, 3)), np.int32)}
        self.assertAlmostEqual(paired_surface_bound(source, xy, carrier, xy), 0.5, 14)


if __name__ == "__main__":
    unittest.main()
