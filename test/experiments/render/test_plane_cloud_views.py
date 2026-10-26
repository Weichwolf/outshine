"""Viewport clipping and preservation of supplied native camera states."""

import unittest
from pathlib import Path

import numpy as np
from compare_plane_cloud import image_roi, select_full_poses
from full_view_reference import load, views


class PlaneViews(unittest.TestCase):
    def test_frustum_crossing_triangles_do_not_require_inside_vertices(self):
        positions = np.array(
            [[-10.0, -10.0, 1.0], [10.0, -10.0, 1.0], [0.0, 10.0, 1.0]]
        )
        view = np.zeros(3), np.eye(3), 32.0, 64, 64
        np.testing.assert_array_equal(
            image_roi({"positions": positions}, view), [[0, 0], [64, 64]]
        )
        positions[:, 2] = -1
        self.assertIsNone(image_roi({"positions": positions}, view))

    def test_selected_frames_are_unchanged_supplied_native_states(self):
        root = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-full-view"
        )
        source, document, context, _ = load(root / "cell.json.gz")
        supplied = views(document, context)
        selected = select_full_poses(source, supplied)
        self.assertEqual(len(selected), 3)
        self.assertEqual(sum(name.startswith("static-") for name in selected), 1)
        self.assertEqual(sum(name.startswith("moving-") for name in selected), 2)
        for name, view in selected.items():
            self.assertIs(view, supplied[name])
            self.assertEqual(view[3:], (1280, 720))
            self.assertEqual(view[2], document["view"]["focal_px"])
            self.assertIsNotNone(image_roi(source, view))


if __name__ == "__main__":
    unittest.main()
