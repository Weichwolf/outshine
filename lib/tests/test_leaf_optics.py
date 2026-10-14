"""Independent interpolation, energy, sheet reciprocity and owned cache replay."""

import json
import subprocess
import sys
import unittest

import numpy as np
from outshine_ref.vegetation import LeafOpticsTable, leaf_diffuse_response
from outshine_ref.vegetation.leaf_optics import gamut_map, optical_mixture
from outshine_ref.vegetation.leaf_pigments import patch_field, pigment_coordinates


def table_values():
    x, y = np.meshgrid(np.linspace(0, 1, 5), np.linspace(0, 1, 5), indexing="ij")
    value = 0.05 + 0.1 * x + 0.07 * y + 0.05 * x * y
    return np.stack(
        (value, value * 0.8, value * 0.4, value * 0.5, value, value * 0.3), axis=-1
    )


class LeafOpticsTests(unittest.TestCase):
    def test_log_bilinear_table_against_analytic_polynomial(self):
        table = LeafOpticsTable(table_values())
        xy = np.random.default_rng(91).uniform(0, 1, (500, 2))
        xy = np.vstack((xy, ((0, 0), (0, 1), (1, 0), (1, 1))))
        pigment = np.expm1(xy * np.log1p((80, 10)))
        pigment[-4:] = ((0, 0), (0, 10), (80, 0), (80, 10))
        x, y = xy.T
        v = 0.05 + 0.1 * x + 0.07 * y + 0.05 * x * y
        expected = np.column_stack((v, v * 0.8, v * 0.4, v * 0.5, v, v * 0.3))
        np.testing.assert_allclose(table.sample(pigment), expected, rtol=0, atol=2e-16)
        self.assertEqual(table.sample(np.empty((0, 2))).shape, (0, 6))

    def test_gamut_scale_against_independent_feasibility_bisection(self):
        rng = np.random.default_rng(68)
        raw = rng.uniform(-0.1, 0.55, (80, 2, 3))
        raw[..., 1] += 0.2
        energy = (raw @ np.array((0.2126, 0.7152, 0.0722))).sum(axis=1)
        raw /= np.maximum(energy / 0.9, 1)[:, None, None]
        mapped, scales = gamut_map(raw.reshape(-1, 6))
        for before, after, actual_scale in zip(raw, mapped.reshape(-1, 2, 3), scales):
            luminance = before @ np.array((0.2126, 0.7152, 0.0722))
            lo, hi = 0.0, 1.0
            for _ in range(60):
                mid = (lo + hi) / 2
                candidate = luminance[:, None] + mid * (before - luminance[:, None])
                if candidate.min() >= 0 and candidate.max(axis=1).sum() <= 1:
                    lo = mid
                else:
                    hi = mid
            self.assertAlmostEqual(actual_scale, lo, places=14)
            np.testing.assert_allclose(
                after @ (0.2126, 0.7152, 0.0722), luminance, atol=2e-16
            )

    def test_mixture_zero_pure_and_quantized_edges(self):
        optical = np.array(
            (
                (0, 0, 0, 0, 0, 0),
                (0.7, 0.2, 0.1, 0, 0, 0),
                (0, 0, 0, 0.1, 0.6, 0.2),
                (0.1, 0.2, 0.03, 0.4, 0.6, 0.1),
            )
        )
        colour, transmitted, weight = optical_mixture(optical)
        np.testing.assert_allclose(
            np.column_stack(
                (colour * (1 - weight[:, None]), transmitted * weight[:, None])
            ),
            optical,
            atol=1e-16,
        )
        self.assertTrue(
            all(
                np.isfinite(a).all() and a.min() >= 0 and a.max() <= 1
                for a in (colour, transmitted, weight)
            )
        )
        c, t, w = (np.rint(a * 255) / 255 for a in (colour, transmitted, weight))
        reconstructed = np.column_stack((c * (1 - w[:, None]), t * w[:, None]))
        self.assertLess(np.max(abs(reconstructed - optical)), 1 / 255)

    def test_table_owned_recipe_and_numpy_only_fresh_process(self):
        values = table_values()
        table = LeafOpticsTable(values)
        values[:] = 0
        with self.assertRaises(ValueError):
            table.values.setflags(write=True)
        recipe = json.loads(json.dumps(table.recipe()))
        restored = LeafOpticsTable.from_recipe(recipe)
        np.testing.assert_array_equal(restored.values, table.values)
        program = "import sys,json; from outshine_ref.vegetation import LeafOpticsTable; t=LeafOpticsTable.from_recipe(json.load(sys.stdin)); assert not any(n in sys.modules for n in ('prosail','colour','scipy','bpy')); print(t.sample([[45,.2]]).tolist())"
        result = subprocess.run(
            [sys.executable, "-c", program],
            input=json.dumps(recipe),
            capture_output=True,
            text=True,
            check=True,
        )
        np.testing.assert_array_equal(
            json.loads(result.stdout), table.sample(((45, 0.2),))
        )

    def test_sheet_front_back_flip_and_reciprocity(self):
        optical = np.array(((0.1, 0.2, 0.03, 0.4, 0.6, 0.1),))
        n, v, l = (0, 0, 1), (0.6, 0, 0.8), (0, 0.8, -0.6)
        response = leaf_diffuse_response(optical, n, v, l)
        np.testing.assert_allclose(response, optical[:, 3:] * 0.6 / np.pi)
        np.testing.assert_array_equal(
            response, leaf_diffuse_response(optical, (0, 0, -1), v, l)
        )
        reverse = leaf_diffuse_response(optical, n, l, v)
        np.testing.assert_allclose(response / 0.6, reverse / 0.8, atol=1e-16)
        np.testing.assert_allclose(
            leaf_diffuse_response(optical, n, v, (0, 0, 1)), optical[:, :3] / np.pi
        )

    def test_sheet_uniform_white_environment_preserves_hemispherical_energy(self):
        x, w = np.polynomial.legendre.leggauss(32)
        z = np.concatenate(((x + 1) / 2, -(x + 1) / 2))
        weights = np.tile(w / 2, 2) * 2 * np.pi
        lights = np.column_stack((np.sqrt(1 - z * z), np.zeros(len(z)), z))
        optical = np.tile((0.2, 0.1, 0.05, 0.4, 0.2, 0.03), (len(z), 1))
        response = leaf_diffuse_response(optical, (0, 0, 1), (0, 0, 1), lights)
        np.testing.assert_allclose(
            weights @ response, (0.6, 0.3, 0.08), rtol=0, atol=3e-16
        )

    def test_pigment_fields_batch_order_and_physical_coordinate_stability(self):
        points = np.random.default_rng(55).uniform(-0.04, 0.04, (1000, 2))
        a = pigment_coordinates(points, np.zeros(1000), 121, 45, 0.2)
        b = pigment_coordinates(points[::-1], np.zeros(1000), 121, 45, 0.2)
        np.testing.assert_array_equal(a, b[::-1])
        np.testing.assert_array_equal(
            a,
            np.concatenate(
                [
                    pigment_coordinates(p, np.zeros(len(p)), 121, 45, 0.2)
                    for p in np.array_split(points, 17)
                ]
            ),
        )
        self.assertFalse(
            np.array_equal(patch_field(points, 121), patch_field(points, 122))
        )

    def test_invalid_cache_colours_geometry_and_domains_reject(self):
        for values in (
            np.zeros((2, 2, 6)),
            np.full((5, 5, 6), np.nan),
            table_values().astype(complex),
            np.full((5, 5, 6), 0.6),
        ):
            with self.assertRaises(ValueError):
                LeafOpticsTable(values)
        table = LeafOpticsTable(table_values())
        for pigments in (
            ((81, 0),),
            ((0, -1),),
            ((np.nan, 0),),
            np.zeros((262145, 2)),
            np.zeros(2),
        ):
            with self.assertRaises(ValueError):
                table.sample(pigments)
        with self.assertRaises(ValueError):
            optical_mixture(((0.6, 0, 0, 0, 0.6, 0),))
        with self.assertRaises(ValueError):
            leaf_diffuse_response(((0.1,) * 6,), (0, 0, 0), (0, 0, 1), (0, 0, 1))
        with self.assertRaises(ValueError):
            pigment_coordinates(((0, 0),), (1.1,), 121, 45, 0.2)
        with self.assertRaises(ValueError):
            patch_field(((0, 0),), True)
        recipe = table.recipe() | {"version": "leaf-optics-table-2"}
        with self.assertRaises(ValueError):
            LeafOpticsTable.from_recipe(recipe)
