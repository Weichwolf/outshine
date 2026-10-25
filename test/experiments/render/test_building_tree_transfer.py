"""Depth composition and first-hit coverage must not count a group twice."""

import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from building_mass_inputs import MassSource
from building_tree_products import projected_mass_coverage
from building_tree_transfer import compose_layers, transfers
from compare_mass_appearance import read


class BuildingTreeTransfer(unittest.TestCase):
    def test_depth_order_clear_opaque_and_fractional_layers(self):
        # Unsorted records: green behind half-red, clear blue before opaque yellow.
        colour, transmission, layers = compose_layers(
            np.array([0, 1, 0, 1, 1]),
            np.array([2, 0, 1, 1, 2.0]),
            np.array([[0, 1, 0], [0, 0, 1], [1, 0, 0], [1, 1, 0], [1, 0, 1.0]]),
            np.array([0.25, 0, 0.5, 1, 1.0]),
            np.arange(5),
            3,
        )
        np.testing.assert_array_equal(colour, [[0.5, 0.125, 0], [1, 1, 0], [0, 0, 0]])
        np.testing.assert_array_equal(transmission, [0.375, 0, 1])
        self.assertEqual(layers, 3)

    def test_coplanar_opaque_faces_use_global_face_tie_order(self):
        colour, t, _ = compose_layers(
            np.array([0, 0]), np.ones(2), np.eye(3)[:2], np.ones(2), [7, 3], 1
        )
        np.testing.assert_array_equal(colour, [[0, 1, 0]])
        np.testing.assert_array_equal(t, [0])
        with self.assertRaises(ValueError):
            compose_layers(np.array([0]), [1], [[1, 0, 0]], [1.1], [0], 1)

    def test_source_projection_measures_union_instead_of_summing_overlaps(self):
        # Two half-overlapping prisms occupy3/4 of the proxy's top/side extent.
        sources = [
            MassSource(i, shapely.box(x, 0, x + 2, 1), (0, 0), (4, 4))
            for i, x in ((1, 0), (2, 1))
        ]
        coverage, raw = projected_mass_coverage(
            sources, shapely.box(0, 0, 4, 1), [0, 4]
        )
        np.testing.assert_array_equal(coverage, [0.75, 0.75, 1])
        np.testing.assert_array_equal(raw, coverage)

    def test_two_front_surfaces_of_one_group_only_apply_coverage_once(self):
        positions = np.array(
            [[-2, -2, 4], [0, 2, 4], [2, -2, 4], [-3, -3, 6], [0, 3, 6], [3, -3, 6.0]]
        )
        candidate = {
            "positions": positions,
            "indices": np.array([[0, 2, 1], [3, 5, 4]]),
        }
        product = candidate | {
            "triangle_groups": np.array([0, 0]),
            "group_coverage": np.full((1, 3), 0.25),
            "needs_sources": np.empty(0, np.uint32),
        }
        appearance = {
            "albedo": np.ones((1, 2, 3)),
            "roughness_weights": np.ones((1, 2, 1)),
            "roughness_palette": np.array([0.7]),
            "supported": np.ones(1, bool),
        }
        camera = (np.zeros(3), np.eye(3), 1, 1, 1)
        original = {"binding": np.empty((0, 6), int)}
        tables = read(
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/material-filter/energy-tables.npz"
        )
        with patch(
            "building_tree_transfer.shade",
            return_value=(
                np.ones((1, 3)),
                {"ggx_lobe_evaluations": 1, "maximum_row_array_bytes": 0},
            ),
        ):
            image, work = transfers(
                candidate,
                original,
                {},
                product,
                appearance,
                camera,
                np.array([[0, 0], [1, 1]]),
                tables,
                1,
            )
        np.testing.assert_array_equal(image["C"], [[[0.25, 0.25, 0.25]]])
        np.testing.assert_array_equal(image["T"], [[[0.75]]])
        self.assertEqual(work["layer_records"], 1)


if __name__ == "__main__":
    unittest.main()
