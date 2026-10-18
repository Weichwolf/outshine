"""Original-UV, conditional mass and material-identity counterexamples."""

import unittest
from dataclasses import asdict

import numpy as np
from blade_surface import Blade
from connected_leaf import build_connected
from connected_material_fields import (
    build_material,
    field_identity,
    filtered_fields,
    repeated_image,
    validate_fields,
)
from outshine_ref.vegetation.leaf_normals import resolve_moments


class ConnectedMaterialChecks(unittest.TestCase):
    def test_repeat_bilinear_matches_independent_scalar_at_wraps_and_negative_uv(self):
        image = np.random.default_rng(704).random((7, 11, 3))
        uv = np.random.default_rng(705).uniform(-2, 2, (123, 2))
        uv = np.vstack((uv, ((0, 0), (1, 1), (-1, -1))))
        actual = repeated_image(image, uv)
        expected = []
        for u, v in uv:
            x, y = 11 * u - 0.5, 7 * v - 0.5
            i, j = int(np.floor(x)), int(np.floor(y))
            a, b = x - i, y - j
            expected.append(
                sum(
                    image[(j + dy) % 7, (i + dx) % 11]
                    * (a if dx else 1 - a)
                    * (b if dy else 1 - b)
                    for dy in (0, 1)
                    for dx in (0, 1)
                )
            )
        np.testing.assert_allclose(actual, expected, rtol=0, atol=3e-16)
        np.testing.assert_array_equal(actual[-3], actual[-2])
        np.testing.assert_array_equal(actual[-2], actual[-1])

    def test_partial_coverage_preserves_conditional_colour_height_and_slope_variance(
        self,
    ):
        mask = np.array(((True, False), (True, False)))
        slopes = np.array((((0.5, 0), (99, 99)), ((-0.5, 0), (99, 99))))
        colour = np.full((2, 2, 3), (0.2, 0.4, 0.6))
        height = np.full((2, 2), 0.75)
        fields = filtered_fields(mask, slopes, colour, height, 2)
        a, b = fields["slope_a"][0, 0], fields["slope_b"][0, 0]
        np.testing.assert_array_equal(a, (0, 0, 0.125, 0.5))
        np.testing.assert_array_equal(b, (0, 0, 0.375, 0.5))
        np.testing.assert_allclose(fields["colour"][0, 0], (0.1, 0.2, 0.3, 0.5))
        moments = np.array((a[0], a[1], a[2], b[0], b[1], 0.5 * 0.55**4)) / a[3]
        normal, broad, covariance = resolve_moments(moments)
        _, naive, _ = resolve_moments(moments, broaden=False)
        np.testing.assert_array_equal(normal, (0, 0, 1))
        np.testing.assert_allclose(covariance, (0.25, 0, 0))
        self.assertGreater(broad, naive)
        self.assertAlmostEqual(naive, 0.55)

    def test_constant_original_maps_and_geometry_domain_are_preserved(self):
        recipe = build_connected(Blade(), size=32)["recipe"]
        colour = np.full((8, 8, 3), (0.1, 0.2, 0.3))
        height = np.full((8, 8), 0.6)
        arrays, description = build_material(
            recipe, colour, height, material_identity="original"
        )
        alpha = arrays["colour"][..., 3]
        np.testing.assert_allclose(
            arrays["colour"][..., :3], alpha[..., None] * (0.1, 0.2, 0.3), atol=1e-16
        )
        np.testing.assert_allclose(arrays["slope_b"][..., 2], alpha * 0.6, atol=1e-16)
        for key in ("slope_a", "slope_b"):
            np.testing.assert_array_equal(arrays[key][..., 3], alpha)
        self.assertGreater(float(alpha.sum()), 0)
        _, changed = build_material(recipe, colour, height, material_identity="changed")
        self.assertNotEqual(description["identity"], changed["identity"])
        self.assertEqual(description["carrier_recipe"], recipe)

    def test_source_slope_chart_refuses_uncovered_hemisphere(self):
        recipe = {
            "blade": asdict(Blade(bend=2.9)),
            "source_mesh": [8, 1],
            "map_size": 32,
        }
        with self.assertRaisesRegex(ValueError, "positive object-Z"):
            build_material(
                recipe,
                np.ones((8, 8, 3)),
                np.zeros((8, 8)),
                material_identity="original",
            )

    def test_finished_field_content_and_covariance_are_checked(self):
        recipe = build_connected(Blade(), size=32)["recipe"]
        arrays, description = build_material(
            recipe,
            np.full((8, 8, 3), 0.2),
            np.zeros((8, 8)),
            material_identity="original",
        )
        validate_fields(arrays)
        self.assertEqual(field_identity(arrays, description), description["identity"])
        changed = {k: v.copy() for k, v in arrays.items()}
        changed["colour"][..., 0] *= 0.5
        validate_fields(changed)
        self.assertNotEqual(
            field_identity(changed, description), description["identity"]
        )
        changed["slope_a"][..., 2] = -1
        with self.assertRaises(ValueError):
            validate_fields(changed)


if __name__ == "__main__":
    unittest.main()
