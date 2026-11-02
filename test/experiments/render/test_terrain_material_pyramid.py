"""Spatial selection, joint-response filtering and immutable material residency."""

import unittest
from unittest.mock import Mock

import numpy as np
import shapely
from relief_shading_reference import direction
from terrain_material_filter import response
from terrain_material_pyramid import bake, filtered_components, sample


def two_materials(at, dx, dy):
    red = at[:, 0] < 0.5
    gradient = np.column_stack((np.where(red, 1, -1), np.zeros(len(at))))
    return np.column_stack(
        (
            red,
            np.zeros(len(at)),
            ~red,
            np.full(len(at), 0.5),
            np.sum(gradient * dx, axis=-1),
            np.sum(gradient * dy, axis=-1),
        )
    )


def query(pyramid, at, dx, dy, lod):
    fields, weights, _ = filtered_components(pyramid, at, dx, dy, lod)
    return response(
        fields,
        weights,
        np.array([direction(0), direction(70), direction(80, 90)]),
        direction(30),
    )


class MaterialPyramidTests(unittest.TestCase):
    def test_build_once_then_move_rotate_and_change_lod_without_generation(self):
        evaluate = Mock(side_effect=two_materials)
        pyramid = bake(evaluate, [0, 0], 1, 8)
        before = tuple(
            (level.fields.tobytes(), level.weights.tobytes())
            for level in pyramid.levels
        )
        for lod in (0, 0.3, 1, 1.9, 3):
            query(pyramid, [0.5, 0.5], [0.5, 0.1], [-0.05, 0.25], lod)
            query(pyramid, [0.3, 0.4], [0.1, -0.2], [0.1, 0.05], lod)
        evaluate.assert_called_once()
        self.assertEqual(
            before,
            tuple(
                (level.fields.tobytes(), level.weights.tobytes())
                for level in pyramid.levels
            ),
        )
        self.assertEqual(
            pyramid.retained_bytes, (8 * 8 + 4 * (4 * 4 + 2 * 2 + 1)) * 7 * 4
        )
        for level in pyramid.levels:
            with self.assertRaises(ValueError):
                level.fields.flags.writeable = True
            with self.assertRaises(ValueError):
                level.weights.flags.writeable = True

    def test_two_correlated_materials_keep_full_tile_light_at_every_lod(self):
        pyramid = bake(two_materials, [0, 0], 1, 8, capacity=2)
        expected = query(pyramid, [0.5, 0.5], [1, 0], [0, 1], 0)
        for lod in (0.3, 1, 1.9, 3):
            np.testing.assert_allclose(
                query(pyramid, [0.5, 0.5], [1, 0], [0, 1], lod), expected, atol=2e-15
            )

    def test_fine_level_matches_independent_rotated_anisotropic_area(self):
        pyramid = bake(two_materials, [0, 0], 1, 8)
        at, dx, dy = (
            np.array([0.53, 0.5]),
            np.array([0.5, 0.2]),
            np.array([-0.01, 0.025]),
        )
        fields, weights, cells = filtered_components(pyramid, at, dx, dy, 0)
        polygon = shapely.Polygon(
            [
                at - (dx + dy) / 2,
                at + (dx - dy) / 2,
                at + (dx + dy) / 2,
                at + (dy - dx) / 2,
            ]
        )
        fraction = polygon.intersection(shapely.box(0, 0, 0.5, 1)).area / polygon.area
        self.assertAlmostEqual(float(weights @ fields[:, 0]), fraction, places=13)
        self.assertAlmostEqual(float(weights.sum()), 1, places=14)
        self.assertGreater(cells, 1)

    def test_lod_blends_responses_without_interpolating_normal_directions(self):
        pyramid = bake(two_materials, [0, 0], 1, 8)
        args = (pyramid, [0.4, 0.5], [0.25, 0], [0, 0.25])
        fine, coarse = query(*args, 2), query(*args, 3)
        np.testing.assert_allclose(
            query(*args, 2.37), 0.63 * fine + 0.37 * coarse, atol=1e-15
        )

    def test_refuses_missing_tile_and_invalid_footprint(self):
        pyramid = bake(two_materials, [0, 0], 1, 8)
        for at, dx, dy, lod in (
            ([0, 0], [0.1, 0], [0, 0.1], 0),
            ([0.5, 0.5], [0.1, 0], [0, 0.1], 4),
            ([0.5, 0.5], [0.1, 0], [0.2, 0], 0),
            ([0.5, np.nan], [0.1, 0], [0, 0.1], 0),
        ):
            with self.subTest(at=at, lod=lod), self.assertRaises(ValueError):
                filtered_components(pyramid, at, dx, dy, lod)

    def test_bad_admission_does_not_evaluate(self):
        evaluate = Mock()
        for options in (
            {"side": 6},
            {"side": 256},
            {"capacity": 9},
            {"capacity": True},
            {"extent_m": -1},
            {"dtype": "int16"},
            {"origin_m": [0, np.inf]},
        ):
            args = {"origin_m": [0, 0], "extent_m": 1, "side": 8}
            args.update(options)
            with self.subTest(options=options), self.assertRaises(ValueError):
                bake(evaluate, **args)
        evaluate.assert_not_called()

    def test_one_sample_matches_joint_mip_and_spatial_expectation(self):
        pyramid = bake(two_materials, [0, 0], 1, 8, capacity=2)
        u = np.stack(
            np.meshgrid(
                [0.25, 0.75],
                (np.arange(16) + 0.5) / 16,
                (np.arange(16) + 0.5) / 16,
                (np.arange(16) + 0.5) / 16,
            ),
            axis=-1,
        ).reshape(-1, 4)
        count = len(u)
        raw = sample(
            pyramid,
            np.tile([0.5, 0.5], (count, 1)),
            np.tile([1, 0], (count, 1)),
            np.tile([0, 1], (count, 1)),
            np.full(count, 2.5),
            u,
        )
        lights, view = (
            np.array([direction(0), direction(70), direction(80, 90)]),
            direction(30),
        )
        np.testing.assert_allclose(
            response(raw, np.ones(count) / count, lights, view),
            query(pyramid, [0.5, 0.5], [1, 0], [0, 1], 2.5),
            atol=1e-14,
        )

    def test_sampling_refuses_invalid_random_values_and_missing_coverage(self):
        pyramid = bake(two_materials, [0, 0], 1, 8)
        for at, random in (
            ([[0, 0]], [[0.5] * 4]),
            ([[0.5, 0.5]], [[1, 0.5, 0.5, 0.5]]),
            ([[0.5, 0.5]], [[0.5, np.nan, 0.5, 0.5]]),
        ):
            with self.subTest(at=at, random=random), self.assertRaises(ValueError):
                sample(pyramid, at, [[0.1, 0]], [[0, 0.1]], [0], random)


if __name__ == "__main__":
    unittest.main()
