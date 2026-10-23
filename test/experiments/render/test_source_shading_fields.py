"""Weld validity, source normal seams, native colour predicates and exact transport."""

import unittest

import numpy as np
from compare_source_material_fields import (
    diagnostic_pixels,
    field_errors,
    sample_fields,
)
from coupled_vertex_clusters import cluster_mesh
from packed_normals import unpack
from prepared_material_fields import facade_catalog
from source_material_mesh import carry_fields, material_mesh
from source_material_patches import shape_material_patches
from source_shading_fields import (
    curved_normals,
    pack_float32,
    shading_fields,
    wall_colours,
)
from test_source_material_mesh import prepared_shape, surface


def patches(*, colour=None, rotated=False):
    s = surface() | {"wall_colour": colour}
    if rotated:
        s |= {"east": [0, 1, 0], "north": [-1, 0, 0]}
    shape = prepared_shape() | {"form": 1, "round_footprint": False}
    return shape_material_patches(s, shape, 42, 0, np.eye(3), np.zeros(3))


class SourceShadingTest(unittest.TestCase):
    def test_constant_colour_diagnostic_does_not_report_barycentric_noise(self):
        fields = {
            "face": np.zeros(4, int),
            "colour": np.tile([1 - 2e-16, 1, 1 + 2e-16], (4, 1)),
        }
        np.testing.assert_array_equal(
            diagnostic_pixels(fields, "colour", 1, 1), [[[255, 255, 255]]]
        )

    def test_unknown_weld_normal_is_not_admitted_by_the_actual_hit_reader(self):
        source = {
            "positions": np.array([[0, 0, 1], [1, 0, 1], [0, 1, 1]], float),
            "indices": np.array([[0, 1, 2]]),
            "vertex_normal": np.tile([0.0, 0.0, 1.0], (3, 1)),
            "uv": np.zeros((3, 2)),
            "metres": np.zeros((3, 2)),
            "colour": np.ones((3, 4)),
        }
        origins, directions = np.array([[0.2, 0.2, 3]]), np.array([[0, 0, -1]])
        unknown, _ = sample_fields(
            source, origins, directions, qualified_faces=np.array([False])
        )
        known, _ = sample_fields(source, origins, directions)
        self.assertEqual(unknown["face"][0], 0)
        self.assertFalse(unknown["shading_valid"][0])
        np.testing.assert_array_equal(unknown["normal"], 0)
        self.assertEqual(field_errors(known, unknown)["qualified_normal_rays"], 0)

    def test_native_oct_axis_words_and_invalid_inputs(self):
        axes = np.array(
            [[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]],
            np.float32,
        )
        np.testing.assert_array_equal(
            pack_float32(axes),
            [0xFFFF8000, 0x00008000, 0x8000FFFF, 0x80000000, 0x80008000, 0xFFFFFFFF],
        )
        for bad in (
            np.zeros((1, 3), np.float32),
            np.ones((1, 3)),
            np.array([[np.nan, 1, 0]], np.float32),
        ):
            with self.assertRaises(ValueError):
                pack_float32(bad)

    def test_rotated_source_frame_and_reversed_parameter_winding_agree(self):
        wall = next(p for p in patches(rotated=True) if p.side == 2 and not p.gable)
        q = np.array([[[0, 12], [4, 12], [4, 16]]])
        a, b = shading_fields(wall, q), shading_fields(wall, q[:, [0, 2, 1]])
        self.assertTrue(a["face_shading_valid"].all())
        np.testing.assert_array_equal(a["corner_shading_words"], 0xFFFF8000)
        np.testing.assert_array_equal(
            a["corner_shading_words"], b["corner_shading_words"]
        )
        np.testing.assert_array_equal(a["face_native_stream"], [0])

    def test_submillimetre_face_stays_explicitly_unqualified(self):
        wall = next(p for p in patches() if p.side == 2 and not p.gable)
        result = shading_fields(wall, [[[0, 12], [0.0001, 12], [0, 12.0001]]])
        self.assertFalse(result["face_shading_valid"].any())
        np.testing.assert_array_equal(result["corner_shading_words"], 0)
        np.testing.assert_array_equal(result["face_native_stream"], [-1])

    def test_native_wall_colour_does_not_paint_glass_trim_or_roof_stream(self):
        c = facade_catalog()
        uv = np.array(
            [
                [256.5, 2],
                [256 * c["Glazing"] + 0.5, 2],
                [256 * (1 + len(c)) + 0.5, 2],
                [-1, 2],
                [-6, 2],
                [256.5, 2],
            ],
            np.float32,
        )
        result = wall_colours(uv, [0, 0, 0, 0, 0, 1], [1.48, 1.42, 1.30])
        np.testing.assert_array_equal(result[[0, 3], :3], 2)
        np.testing.assert_array_equal(result[[1, 2, 4, 5]], 1)
        np.testing.assert_array_equal(result[:, 3], 1)
        np.testing.assert_array_equal(wall_colours(uv, 0, None), 1)

    def test_ellipse_uses_point_gradient_only_for_native_curved_wall_case(self):
        shape = prepared_shape() | {
            "form": 5,
            "opening_style": 5,
            "holes": [],
            "round_footprint": True,
            "ring": [[0, 0]] * 8,
            "centre": [0, 0],
            "half_u_m": 2,
            "half_v_m": 1,
        }
        points = np.array([[[2, 0, 0], [1, 1, 0], [0, 1, 0]]], float)
        geometric = np.array([[1, 0, 0]], float)
        actual = curved_normals(shape, points, geometric, catalog=facade_catalog())
        expected = np.array(
            [[[1, 0, 0], [1 / np.sqrt(17), 4 / np.sqrt(17), 0], [0, 1, 0]]]
        )
        np.testing.assert_allclose(actual, expected, rtol=1e-15)
        flat = curved_normals(
            shape | {"opening_style": 1}, points, geometric, catalog=facade_catalog()
        )
        np.testing.assert_array_equal(
            flat, np.broadcast_to(geometric[:, None], points.shape)
        )
        roof = np.array([[0, 0, 1.0]])
        np.testing.assert_array_equal(
            curved_normals(shape, points, roof, catalog=facade_catalog()),
            np.broadcast_to(roof[:, None], points.shape),
        )

    def test_optional_shading_keeps_geometry_and_default_colour_without_corner_ones(
        self,
    ):
        old, old_fields, _ = material_mesh(patches())
        geometry, fields, work = material_mesh(patches(), with_shading=True)
        for key in geometry:
            np.testing.assert_array_equal(geometry[key], old[key])
        for key in old_fields:
            np.testing.assert_array_equal(fields[key], old_fields[key])
        self.assertNotIn("corner_colour", fields)
        np.testing.assert_array_equal(fields["uniform_colour"], np.ones(4, np.float32))
        self.assertTrue(work["colour_payload_is_uniform"])
        centre, _ = cluster_mesh(
            geometry["positions"], geometry["indices"], method="centre"
        )
        recovered = carry_fields(
            carry_fields(fields, centre["corner_swap"]), centre["corner_swap"]
        )
        np.testing.assert_array_equal(
            recovered["corner_shading_words"], fields["corner_shading_words"]
        )
        valid = fields["face_shading_valid"]
        self.assertTrue(
            np.isfinite(unpack(fields["corner_shading_words"][valid].reshape(-1))).all()
        )

    def test_coloured_source_retains_actual_rgba_corner_values(self):
        _, fields, work = material_mesh(
            patches(colour=[1.48, 1.42, 1.30]), with_shading=True
        )
        self.assertNotIn("uniform_colour", fields)
        self.assertFalse(work["colour_payload_is_uniform"])
        np.testing.assert_array_equal(
            fields["corner_colour"][fields["face_native_stream"] == 0, :, :3], 2
        )
        np.testing.assert_array_equal(
            fields["corner_colour"][fields["face_native_stream"] == 1], 1
        )


if __name__ == "__main__":
    unittest.main()
