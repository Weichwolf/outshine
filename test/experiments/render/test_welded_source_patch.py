"""Source floor completeness, native cast stages and exact collapsed-weld checks."""

import math
import struct
import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from complete_source_patches import complete_shape_patches
from prepared_material_fields import at_local
from source_material_patches import corner_fields
from test_source_material_mesh import prepared_shape, surface
from welded_source_patch import (
    TriangleBudget,
    patch_corners,
    source_corners,
    stored_positions,
)


def float32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


class WeldedSourcePatch(unittest.TestCase):
    def test_source_floor_preserves_hole_downward_winding_and_native_fields(self):
        shape = prepared_shape() | {"form": 0, "round_footprint": False}
        shape["holes"] = [[[2, 1], [2, 3], [6, 3], [6, 1]]]
        parent = surface() | {"wall_colour": None}
        patches = complete_shape_patches(parent, shape, 42, 0, np.eye(3), np.zeros(3))
        floor = next(p for p in patches if p.side == 0)
        self.assertEqual(len(floor.geometry.domain.interiors), 1)
        self.assertEqual(floor.geometry.domain.area, 24)
        self.assertEqual(floor.orientation, -1)
        parameters = np.array([[0, 0], [8, 4], [2, 1]], float)
        uv, metres, _ = corner_fields(floor, parameters)
        expected = at_local(
            shape, 0, floor.local_origin + parameters @ floor.local_axes
        )
        np.testing.assert_array_equal(uv, expected[0])
        np.testing.assert_array_equal(metres, expected[1])
        corners, work = patch_corners(floor, np.eye(3))
        cross = np.cross(
            corners["positions"][:, 1] - corners["positions"][:, 0],
            corners["positions"][:, 2] - corners["positions"][:, 0],
        )
        self.assertTrue(np.all(cross[:, 2] < 0))
        self.assertTrue(np.all(corners["normals"][..., 2] < -0.999))
        self.assertTrue(np.all(corners["material"] == 0))
        self.assertEqual(work["native_rule_pruned_degenerate_triangles"], 0)

    def test_stored_ecef_rounding_precedes_capture_transform(self):
        patch = SimpleNamespace(
            local_origin=np.zeros(3),
            local_axes=np.array([[1, 0, 0], [0, 1, 0]], float),
            surface={
                "origin": [4096.0001, 2048.0002, 3.0],
                "east": [1, 0, 0],
                "north": [0, 1, 0],
                "up": [0, 0, 1],
            },
        )
        q = np.array([[0, 0], [0.0005, -0.0005], [1.25, 0.0001]], float)
        basis = np.array([[0.6, 0.8, 0], [-0.8, 0.6, 0], [0, 0, 1]])
        actual, _ = stored_positions(patch, q, basis)
        expected = []
        for row in q:
            snapped = [
                math.copysign(math.floor(abs(x * 1000) + 0.5), x) / 1000 for x in row
            ]
            stored = [
                float32(patch.surface["origin"][i] + (snapped[i] if i < 2 else 0))
                for i in range(3)
            ]
            expected.append(
                [
                    float32(
                        (stored[0] * axis[0] + stored[1] * axis[1])
                        + stored[2] * axis[2]
                    )
                    for axis in basis
                ]
            )
        np.testing.assert_array_equal(
            actual.view("u4"), np.array(expected, np.float32).view("u4")
        )
        origin = patch.surface["origin"]
        bypassed = float32(
            (origin[0] * basis[1, 0] + origin[1] * basis[1, 1])
            + origin[2] * basis[1, 2]
        )
        self.assertNotEqual(actual[0, 1], bypassed)

    def test_triangle_budget_rejects_before_attributes_and_counts_whole_source(self):
        shape = prepared_shape() | {"form": 0, "round_footprint": False}
        parent = surface() | {"wall_colour": None}
        patches = complete_shape_patches(parent, shape, 42, 0, np.eye(3), np.zeros(3))
        baseline, work = source_corners(patches, np.eye(3))
        bounded, _ = source_corners(
            patches, np.eye(3), max_triangles=work["domain_triangles"]
        )
        for key in baseline:
            self.assertEqual(baseline[key].tobytes(), bounded[key].tobytes())
        with self.assertRaises(TriangleBudget):
            source_corners(
                patches, np.eye(3), max_triangles=work["domain_triangles"] - 1
            )
        with (
            patch("welded_source_patch.corner_fields", side_effect=AssertionError),
            self.assertRaises(TriangleBudget),
        ):
            patch_corners(patches[0], np.eye(3), max_triangles=0)
        for limit in (-1, True, 0.5, 131073):
            with self.assertRaises(ValueError):
                source_corners(patches, np.eye(3), max_triangles=limit)

    def test_only_exactly_collapsed_local_weld_faces_are_pruned(self):
        shape = prepared_shape() | {"form": 0, "round_footprint": False}
        parent = surface() | {"wall_colour": None}
        floor = next(
            p
            for p in complete_shape_patches(
                parent, shape, 42, 0, np.eye(3), np.zeros(3)
            )
            if p.side == 0
        )
        floor.geometry.domain = shapely.Polygon([[0, 0], [0.0004, 0], [0, 0.0004]])
        result, work = patch_corners(floor, np.eye(3))
        self.assertEqual(len(result["material"]), 0)
        self.assertEqual(work["domain_triangles"], 1)
        self.assertEqual(work["native_rule_pruned_degenerate_triangles"], 1)


if __name__ == "__main__":
    unittest.main()
