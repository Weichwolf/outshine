"""Own fin anchor, differential warp, normal law, budget and ownership contracts."""

import json
import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import (
    Blade,
    CubicSpine,
    FinAttachment,
    Profile,
    SpatialAxis,
    SpineField,
)


def parent(ellipse=(1, 1), curved=False):
    end, slope = (0.1, 0.2) if curved else (0, 0)
    return SpatialAxis(
        CubicSpine.from_hermite(
            [[0, 0, 0], [0, 1.6, end]], [[0, 1.6, 0], [0, 1.6, slope]]
        ),
        Profile(np.full(4, 0.1)),
        ellipse=ellipse,
    )


def template(segments=48, across=4):
    return Blade(
        width=0.08,
        length=0.2,
        base_fill=1,
        fold=0,
        lobes=0,
        petiole=0,
        bend=-0.8,
        twist=0,
    ).mesh(segments, across)


class FinTests(unittest.TestCase):
    def test_circle_mapping_has_an_independent_cylindrical_reference(self):
        fin = FinAttachment(parent(), 0.5, 0.3)
        source = np.array([[-0.09, 0.01, -0.04], [0.023, 0.03, 0.02], [0, 0, 0]])
        angle = 0.3 + source[:, 0] / 0.1
        expected = np.column_stack(
            (
                (0.1 + source[:, 1]) * np.cos(angle),
                0.8 + source[:, 2],
                (0.1 + source[:, 1]) * np.sin(angle),
            )
        )
        np.testing.assert_allclose(fin.positions(source), expected, atol=3e-16)

    def test_curved_elliptical_anchors_follow_the_actual_parent_ring(self):
        axis = parent((1.4, 0.65), True)
        fin = FinAttachment(axis, 0.625, -0.2, np.pi / 2)
        x = np.linspace(-0.05, 0.05, 23)
        points = np.column_stack((x, x * 0, x * 0))
        sample = axis.field.sample(0.625)
        r = axis.radius.evaluate(0.625)
        angle = -0.2 + x / (r * np.sqrt(1.4 * 0.65))
        expected = sample["centre"] + r * (
            1.4 * np.cos(angle)[:, None] * sample["lateral"]
            + 0.65 * np.sin(angle)[:, None] * sample["normal"]
        )
        np.testing.assert_allclose(fin.positions(points), expected, atol=3e-16)

    def test_analytic_jacobian_matches_independent_position_differences(self):
        fin = FinAttachment(parent((1.4, 0.65), True), 0.625, 0.4, 0.5)
        points = np.column_stack(
            (np.linspace(-0.06, 0.06, 41), np.full(41, 0.03), np.full(41, -0.007))
        )
        jacobian = fin.jacobian(points)
        for i in range(3):
            step = np.zeros(3)
            step[i] = 1e-6
            derivative = (
                fin.positions(points + step) - fin.positions(points - step)
            ) / 2e-6
            np.testing.assert_allclose(
                jacobian[:, :, i], derivative, atol=3e-9, rtol=2e-9
            )
        self.assertTrue(np.all(np.linalg.det(jacobian) > 0))

    def test_cofactor_normals_match_solve_and_remain_orthogonal_to_tangents(self):
        fin = FinAttachment(parent((1.4, 0.65), True), 0.625, 0.4, 0.5)
        points = np.column_stack(
            (np.linspace(-0.06, 0.06, 41), np.full(41, 0.03), np.full(41, -0.007))
        )
        a, b = np.array([0.4, 0.7, -0.2]), np.array([-0.3, 0.2, 0.6])
        normal = np.tile(np.cross(a, b), (len(points), 1))
        result = fin.normals(points, normal)
        np.testing.assert_allclose(
            result, fin.normals(points, normal, "solve"), atol=4e-16
        )
        for tangent in (a, b):
            mapped = fin.jacobian(points) @ tangent
            np.testing.assert_allclose(np.sum(result * mapped, axis=1), 0, atol=3e-16)
        np.testing.assert_allclose(
            fin.normals(points, normal * 1e300), result, atol=3e-16
        )

    def test_complete_mesh_retains_uvs_faces_source_data_and_nested_anchor_points(self):
        fin = FinAttachment(parent((1.2, 0.7), True), start_angle=np.pi / 2)
        near_source, coarse_source = template(), template(12, 1)
        saved = {key: value.copy() for key, value in near_source.items()}
        near, coarse = fin.deform(near_source), fin.deform(coarse_source)
        for key, value in saved.items():
            np.testing.assert_array_equal(value, near_source[key])
        for key in ("faces", "parameters"):
            np.testing.assert_array_equal(near[key], near_source[key])
        by_parameter = {
            tuple(uv): p
            for uv, p in zip(near_source["parameters"], near["vertices"], strict=True)
        }
        for uv, p in zip(coarse_source["parameters"], coarse["vertices"], strict=True):
            np.testing.assert_array_equal(p, by_parameter[tuple(uv)])
        axis = (near_source["vertices"][:, 1:] == 0).all(axis=1)
        self.assertGreater(axis.sum(), 2)
        np.testing.assert_array_equal(
            near["vertices"][axis], fin.positions(near_source["vertices"][axis])
        )
        near["parameters"][:] = 0
        np.testing.assert_array_equal(near_source["parameters"], saved["parameters"])

    def test_budget_and_invalid_geometry_fail_before_parent_preparation(self):
        fin = FinAttachment(parent(), start_angle=np.pi / 2)
        source = template()
        with (
            patch.object(
                SpineField,
                "sample",
                side_effect=AssertionError("parent queried"),
            ),
            self.assertRaisesRegex(ValueError, "detail budget"),
        ):
            fin.deform(source, max_faces=1)
        self.assertNotIn("_slice", fin.__dict__)
        self.assertNotIn("field", fin.parent.__dict__)
        alias = {
            "vertices": np.array([[-0.30, 0.01, 0], [0.30, 0.01, 0], [-0.30, 0.1, 0]]),
            "faces": np.array([[0, 1, 2]], dtype=np.int32),
            "normals": np.tile([0, 0, 1], (3, 1)),
        }
        with self.assertRaisesRegex(ValueError, "anchor variation"):
            FinAttachment(parent()).deform(alias)
        for points in ([[0, -0.02, 0]], [[np.pi * 0.1, 0, 0]], [[0, np.nan, 0]]):
            with self.assertRaises(ValueError):
                FinAttachment(parent()).positions(points)
        corrupt = {key: v.copy() for key, v in source.items()}
        corrupt["faces"][0, 0] = len(source["vertices"])
        with self.assertRaises(ValueError):
            fin.deform(corrupt)
        corrupt = {key: v.copy() for key, v in source.items()}
        corrupt["normals"] *= -1
        with self.assertRaisesRegex(ValueError, "oriented surface"):
            fin.deform(corrupt)

    def test_recipe_reload_and_prepared_basis_are_owned_and_immutable(self):
        fin = FinAttachment(parent((1.2, 0.7), True), start_angle=np.pi / 2)
        loaded = FinAttachment.from_recipe(json.loads(json.dumps(fin.recipe())))
        expected, actual = fin.deform(template()), loaded.deform(template())
        for key in expected:
            np.testing.assert_array_equal(expected[key], actual[key])
        with self.assertRaises(ValueError):
            fin._slice[0].setflags(write=True)
        for values in ({"fraction": True}, {"azimuth": np.nan}, {"start_angle": 4}):
            with self.assertRaises(ValueError):
                FinAttachment(parent(), **values)


if __name__ == "__main__":
    unittest.main()
