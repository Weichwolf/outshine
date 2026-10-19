"""Filter contracts and independently soluble GGX cases, not visual admission."""

import unittest

import numpy as np
from ggx_reference import row_reference
from material_filter import direct_ggx, filtered, fixtures


class MaterialFilterTests(unittest.TestCase):
    def test_endpoints_and_constant_albedo_only_materials(self):
        data, descriptions = fixtures()
        self.assertEqual(data.shape, (51, 161, 6, 20))
        for row, description in zip(data, descriptions, strict=True):
            if description["weight"] in (0, 1):
                source = int(description["weight"])
                np.testing.assert_array_equal(row[:, 2], row[:, source])
                np.testing.assert_array_equal(row[:, 3], row[:, source])
        self.assertTrue(np.isfinite(data).all())

    def test_atlas_preserves_alpha_squared_moment_and_normal_variance(self):
        data, desc = fixtures()
        rough = next(
            row
            for row, d in zip(data, desc, strict=True)
            if d == {"kind": "roughness", "weight": 0.5, "view_degrees": 0.0}
        )
        self.assertAlmostEqual(
            float(rough[0, 3, 3]) ** 4, (0.2**4 + 0.8**4) / 2, places=6
        )
        normal = next(
            row
            for row, d in zip(data, desc, strict=True)
            if d == {"kind": "normal", "weight": 0.5, "view_degrees": 0.0}
        )
        self.assertGreater(normal[0, 3, 3], normal[0, 2, 3])
        same = filtered(
            data[0, :, 0].astype(float), data[0, :, 0].astype(float), 0.5, atlas=True
        )
        np.testing.assert_allclose(same, data[0, :, 0], rtol=0, atol=1e-8)

    def test_normal_incidence_roughness_one_has_closed_form(self):
        row = np.zeros((1, 20))
        row[0, :3], row[0, 3], row[0, 4:7], row[0, 7] = [0.4, 0.3, 0.2], 1, 0.04, 1
        row[0, [10, 14, 18]] = 1
        # Directional albedo 1 deliberately disables multiple-scattering scale.
        tables = {
            "shape": (2, 2),
            "kGgxAlbedo": np.ones(4),
            "kGgxAlbedoAverage": np.ones(2),
        }
        actual = direct_ggx(row, tables)
        np.testing.assert_allclose(
            actual[0, :3], 0.96 * row[0, :3] / np.pi, rtol=0, atol=1e-15
        )
        np.testing.assert_allclose(
            actual[0, 4:7], 0.04 / (4 * np.pi), rtol=0, atol=1e-15
        )

    def test_byte_alpha_squared_loses_constant_positive_gloss(self):
        data, descriptions = fixtures()
        row = next(
            row
            for row, d in zip(data, descriptions, strict=True)
            if d["kind"] == "constant_gloss"
        )
        self.assertGreater(row[0, 0, 3], 0)
        self.assertEqual(row[0, 4, 3], 0)
        self.assertEqual(row[0, 5, 3], row[0, 0, 3])

    def test_float64_ggx_against_decimal_for_float32_roughness_extremes(self):
        roughness = np.array(
            [
                0,
                np.finfo(np.float32).smallest_subnormal,
                1e-12,
                1e-6,
                1e-4,
                0.01,
                0.02,
                0.12,
                0.5,
                1,
            ],
            dtype=np.float32,
        ).astype(float)
        tables = {
            "shape": (2, 2),
            "kGgxAlbedo": np.ones(4),
            "kGgxAlbedoAverage": np.ones(2),
        }
        for cosine in (1.0, 0.8):
            for f0 in (0.0, 0.04, 1.0):
                rows = np.zeros((len(roughness), 20))
                rows[:, :3], rows[:, 3], rows[:, 4:7], rows[:, 7] = (
                    [0.4, 0.3, 0.2],
                    roughness,
                    f0,
                    1,
                )
                rows[:, 8:11] = [0, 0, 1]
                sine = np.sqrt(1 - cosine * cosine)
                rows[:, 12:15], rows[:, 16:19] = [sine, 0, cosine], [-sine, 0, cosine]
                with np.errstate(all="raise"):
                    actual = direct_ggx(rows, tables)
                expected = [
                    float(row_reference([r, 1, cosine, cosine, cosine, f0, 0, 0])[-1])
                    for r in roughness
                ]
                np.testing.assert_allclose(
                    actual[:, 4:7],
                    np.repeat(np.array(expected)[:, None], 3, axis=1),
                    rtol=64 * np.finfo(float).eps,
                    atol=0,
                )
                self.assertTrue(np.isfinite(actual).all())


if __name__ == "__main__":
    unittest.main()
