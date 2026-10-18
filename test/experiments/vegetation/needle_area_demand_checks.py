"""Partial query ownership and a geometric counterexample to residual certification."""

import unittest
from unittest.mock import patch

import numpy as np
from needle_area_checks import sheet
from scatter_scene import ScatterScene
from surface_area_cells import clipped_batches
from surface_area_light import integrate, triangle_rule


def positions(source, order):
    bary, _ = triangle_rule(order)
    return np.concatenate(
        [
            np.einsum("ki,nij->nkj", bary, triangles).reshape(-1, 3)
            for triangles, _, _, _ in clipped_batches(
                source, np.array([0.0, 0, 1]), 1, {}
            )
        ]
    )


class NeedleAreaDemandChecks(unittest.TestCase):
    def test_partial_values_match_full_query_and_preserve_explicit_incompleteness(self):
        source = sheet()
        scene = ScatterScene(source["vertices"], source["faces"])
        mask = np.zeros((4, 4), bool)
        mask[1, 2] = True
        mask[3, 1] = True
        full, _ = integrate(source, scene, np.array([0.0, 0, 1]), [[0, 0, 1]], size=4)
        partial, work = integrate(
            source, scene, np.array([0.0, 0, 1]), [[0, 0, 1]], size=4, pixel_mask=mask
        )
        np.testing.assert_array_equal(partial[:, mask], full[:, mask])
        np.testing.assert_array_equal(partial[:, ~mask], np.zeros((3, 14)))
        self.assertFalse(work["complete_footprint"])
        self.assertEqual(work["requested_pixels"], 2)
        self.assertLess(work["requested_candidate_pairs"], work["candidate_pairs"])

    def test_empty_demand_avoids_clipping_and_rays(self):
        source = sheet()
        scene = ScatterScene(source["vertices"], source["faces"])
        with (
            patch(
                "surface_area_cells.projected_source",
                side_effect=AssertionError("clip"),
            ),
            patch.object(scene, "nearest", side_effect=AssertionError("ray")),
        ):
            result, work = integrate(
                source,
                scene,
                np.array([0.0, 0, 1]),
                [[0, 0, 1]],
                size=4,
                pixel_mask=np.zeros((4, 4), bool),
            )
        np.testing.assert_array_equal(result, np.zeros((3, 4, 4)))
        self.assertEqual(work["sampled_points"], 0)

    def test_wrong_grid_or_nonboolean_demand_is_rejected(self):
        source = sheet()
        scene = ScatterScene(source["vertices"], source["faces"])
        for mask in (np.ones((4, 4)), np.ones((3, 3), bool)):
            with self.assertRaisesRegex(ValueError, "boolean pixel demand"):
                integrate(
                    source,
                    scene,
                    np.array([0.0, 0, 1]),
                    [[0, 0, 1]],
                    size=4,
                    pixel_mask=mask,
                )

    def test_false_low_order_agreement_misses_an_actual_shadow(self):
        source = sheet()
        coarse = np.r_[positions(source, 2), positions(source, 4)]
        fine = positions(source, 6)
        fine = fine[(fine[:, 0] < 0.8) & (fine[:, 1] > 0.1) & (fine[:, 1] < 0.9)]
        gaps = abs(fine[:, None, 1] - coarse[None, :, 1]).min(1)
        chosen = int(np.argmax(gaps))
        y = fine[chosen, 1]
        half = gaps[chosen] * 0.4
        blocker = np.array(
            [
                [0.9, y - half, 0],
                [0.9, y + half, 0],
                [0.9, y + half, 1],
                [0.9, y - half, 1],
            ]
        )
        source = {
            "vertices": np.r_[source["vertices"], blocker],
            "faces": np.r_[source["faces"], source["faces"] + 4],
            "normals": np.r_[source["normals"], np.tile([1.0, 0, 0], (2, 3, 1))],
        }
        scene = ScatterScene(source["vertices"], source["faces"])
        light = np.array([1.0, 0, 1]) / np.sqrt(2)
        values = [
            integrate(source, scene, np.array([0.0, 0, 1]), [light], size=1, order=q)[
                0
            ][2, 0, 0]
            for q in (2, 4, 6)
        ]
        exact = (1 - 0.9 * 2 * half) / (1.04**2 * np.sqrt(2))
        self.assertAlmostEqual(values[0], values[1], places=13)
        self.assertGreater(abs(values[1] - exact), 0.002)
        self.assertLess(values[2], values[1])


if __name__ == "__main__":
    unittest.main()
