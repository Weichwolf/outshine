"""Finite camera controls are checked against clipping actual closed triangles."""

import unittest
from unittest.mock import patch

import numpy as np
from demand import View, clip
from test_window_detail_demand import cases
from window_detail_demand import MAX_WINDOWS, WindowDetails
from window_detail_images import CORNERS, camera_candidates, choices, views
from window_extent_plan import compile_bounds
from window_extent_query import WindowBounds


class WindowImageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        plans = cases()
        cls.plans = tuple(plans[i] for i in (0, 5, 8))

    def test_camera_crosses_box_without_containing_any_original_box_corner(self):
        view = View(np.zeros(3), np.array([0.0, 1.0, 0.0]), width=1280, height=720)
        boxes = np.array(
            [
                [[-20.0, 1.0, -20.0], [20.0, 2.0, 20.0]],
                [[-1.0, -3.0, -1.0], [1.0, -2.0, 1.0]],
                [[-0.1, -0.2, -0.1], [0.1, 0.2, 0.1]],
            ]
        )
        metadata = WindowBounds(("crossing", "behind", "near-crossing"), boxes)
        points = np.where(CORNERS, boxes[0, 1], boxes[0, 0])
        distances = ((points - view.eye) @ view.basis.T) @ view.planes[
            :, :3
        ].T + view.planes[:, 3]
        self.assertTrue(np.all(np.any(distances < 0, axis=1)))
        face = np.array(
            [
                [-20.0, 1.0, -20.0],
                [20.0, 1.0, -20.0],
                [20.0, 1.0, 20.0],
                [-20.0, 1.0, 20.0],
            ]
        )
        self.assertGreaterEqual(len(clip(face, view)), 3)
        self.assertEqual(
            camera_candidates(metadata, view), ("crossing", "near-crossing")
        )

    def test_actual_fine_triangle_hits_are_never_lost_by_metadata_camera_control(self):
        for prepared in self.plans:
            metadata = compile_bounds(prepared)
            product = WindowDetails(prepared, max_windows=MAX_WINDOWS)
            draw = product.update(metadata.identifiers)
            self.assertEqual(draw.detailed_ids, metadata.identifiers)
            self.assertFalse(draw.fallback_ids)
            self.assertEqual(len(product.resident), len(prepared.facade.windows))
            for view in views(prepared).values():
                selected = set(camera_candidates(metadata, view))
                for identifier, window in product.resident.items():
                    if identifier in selected:
                        continue
                    points = window.mesh["vertices"][window.mesh["faces"]]
                    self.assertFalse(
                        any(len(clip(triangle, view)) >= 3 for triangle in points),
                        identifier,
                    )

    def test_all_choices_are_resolved_before_any_fine_or_coarse_construction(self):
        prepared = self.plans[0]
        metadata = compile_bounds(prepared)
        with patch(
            "window_detail_images.WindowDetails",
            side_effect=AssertionError("geometry during selection"),
        ):
            for view in views(prepared).values():
                selected = choices(metadata, view)
                self.assertEqual(selected["full"], metadata.identifiers)
                for ids in selected.values():
                    self.assertEqual(
                        tuple(i for i in metadata.identifiers if i in ids), ids
                    )

    def test_empty_and_closed_near_plane_contact(self):
        view = View(np.zeros(3), np.array([0.0, 1.0, 0.0]))
        self.assertEqual(
            camera_candidates(WindowBounds((), np.empty((0, 2, 3))), view), ()
        )
        metadata = WindowBounds(
            ("contact",), np.array([[[-0.01, 0.1, -0.01], [0.01, 0.1, 0.01]]])
        )
        self.assertEqual(camera_candidates(metadata, view), ("contact",))


if __name__ == "__main__":
    unittest.main()
