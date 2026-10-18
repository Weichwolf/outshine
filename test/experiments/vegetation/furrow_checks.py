"""Scalar field, support, periodic metric derivative and cache quantization controls."""

import json
import math
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
from bark_furrows import FurrowRecipe, longitudinal
from furrow_maps import maps, metric_normals
from furrow_probe import export
from furrow_raster import raster, transverse
from PIL import Image


def scalar_value(recipe, plan, x, z):
    def delta(a, b, period):
        return a - b - period * math.floor((a - b) / period + 0.5)

    def cut(q):
        return (1 - q * q) ** 3 if abs(q) < 1 else 0

    value = 0
    theta = 2 * math.pi * z / recipe.height_m
    for index, centre in enumerate(plan["centres"]):
        phases = [
            theta * k + p
            for k, p in zip((1, 2, 4, 7, 13), plan["phases"][index], strict=True)
        ]
        wave = sum(
            math.sin(p) * c
            for p, c in zip(phases, plan["coefficients"][index], strict=True)
        )
        centre += (
            recipe.width_m
            / recipe.furrows
            * 0.2
            * (math.sin(theta) + 0.5 * math.sin(3 * theta))
        )
        centre += plan["wander_m"] * wave
        width = plan["widths"][index] * (0.75 + 0.25 * math.sin(phases[1]))
        depth = plan["depths"][index] * (0.6 + 0.4 * math.sin(phases[0]))
        q = delta(x, centre, recipe.width_m) / width
        value = max(value, depth * (0.65 * cut(q) + 0.35 * cut(q / 3)))
    for index, (cx, cz, length, width, angle, bend, depth) in enumerate(
        plan["fractures"]
    ):
        u, v = delta(x, cx, recipe.width_m), delta(z, cz, recipe.height_m)
        c, s = math.cos(angle), math.sin(angle)
        along = (u * c + v * s) / length
        argument = math.acos(max(-1, min(1, along)))
        jagged = (
            0.9
            * width
            * sum(
                coef * math.cos(k * argument)
                for k, coef in enumerate(plan["fracture_coefficients"][index])
            )
        )
        across = (-u * s + v * c - bend * (1 - along**2) - jagged) / width
        value = max(value, depth * cut(along) * cut(across))
    return value


class FurrowChecks(unittest.TestCase):
    def test_same_complete_field_for_dense_and_spans_including_extreme_aspect(self):
        cases = (
            FurrowRecipe(),
            FurrowRecipe(seed=42),
            FurrowRecipe(furrows=4, fractures=0),
            FurrowRecipe(furrows=64, fractures=1024),
            FurrowRecipe(width_m=2, height_m=0.2, furrows=4),
        )
        for recipe in cases:
            dense, a = raster(recipe, (64, 96), "dense")
            indexed, b = raster(recipe, (64, 96), "indexed")
            np.testing.assert_array_equal(dense, indexed)
            self.assertLess(
                b["primary_pixel_evaluations"] + b["transverse_pixel_evaluations"],
                a["primary_pixel_evaluations"] + a["transverse_pixel_evaluations"],
            )

    def test_independent_scalar_values_at_seeded_points(self):
        for seed in (0, 765, 1001):
            recipe = FurrowRecipe(seed=seed)
            result, _ = raster(recipe, (96, 128))
            plan = recipe.plan()
            for row, column in np.random.default_rng(3).integers(
                (0, 0), (96, 128), (40, 2)
            ):
                expected = scalar_value(
                    recipe,
                    plan,
                    (column + 0.5) * recipe.width_m / 128,
                    (row + 0.5) * recipe.height_m / 96,
                )
                self.assertAlmostEqual(result[row, column], expected, delta=2e-14)

    def test_periodic_furrows_and_fractures_wrap_both_edges(self):
        recipe = FurrowRecipe()
        plan = recipe.plan()
        z = np.linspace(-0.1, 1.3, 50)
        for i in (0, 3, 17):
            for a, b in zip(
                longitudinal(recipe, plan, i, z),
                longitudinal(recipe, plan, i, z + recipe.height_m),
                strict=True,
            ):
                np.testing.assert_allclose(a, b, atol=3e-15, rtol=1e-12)
        plan["fractures"][0] = (0, 0, 0.04, 0.015, 0, 0, 0.004)
        plan["fracture_coefficients"][0] = 0
        arrays = []
        for method in ("dense", "indexed"):
            field = np.zeros((128, 128))
            transverse(recipe, plan, field, method)
            arrays.append(field)
        np.testing.assert_array_equal(*arrays)
        self.assertGreater(arrays[0][0, 0], 0)
        self.assertGreater(arrays[0][-1, -1], 0)

    def test_analytic_metric_normal_and_uniform_scale(self):
        h, w = 96, 160
        z, x = np.meshgrid(np.arange(h) / h, np.arange(w) / w, indexing="ij")
        height = 0.001 * (np.sin(2 * np.pi * x) + 0.3 * np.cos(2 * np.pi * z))
        dx = 0.001 * np.cos(2 * np.pi * x) * np.sin(2 * np.pi / w) / (0.6 / w)
        dz = -0.0003 * np.sin(2 * np.pi * z) * np.sin(2 * np.pi / h) / (1.2 / h)
        reference = np.stack((-dx, -dz, np.ones_like(height)), axis=2)
        reference /= np.linalg.norm(reference, axis=2)[:, :, None]
        normal = metric_normals(height, 0.6, 1.2)
        np.testing.assert_allclose(normal, reference, atol=2e-15)
        np.testing.assert_allclose(
            metric_normals(height * 7, 4.2, 8.4), normal, atol=2e-15
        )

    def test_repetition_seam_is_an_ordinary_normal_neighbourhood(self):
        recipe = FurrowRecipe()
        relief, _ = raster(recipe, (96, 128))
        height = maps(recipe, relief)["height_m"]
        extended = np.tile(height, (3, 3))
        dy, dx = np.gradient(extended, recipe.height_m / 96, recipe.width_m / 128)
        normal = np.stack((-dx, -dy, np.ones_like(dx)), axis=2)[96:192, 128:256]
        normal /= np.linalg.norm(normal, axis=2)[:, :, None]
        np.testing.assert_allclose(metric_normals(height, 0.6, 1.2), normal, atol=1e-7)

    def test_cache_round_trip_orientation_ranges_and_quantization(self):
        recipe = FurrowRecipe()
        relief, _ = raster(recipe, (96, 128))
        fields = maps(recipe, relief)
        with tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-furrow-check-", dir="/tmp"
        ) as directory:
            folder = Path(directory)
            export(recipe, fields, folder)
            metadata = json.loads((folder / "material.json").read_text())
            height = (
                np.asarray(Image.open(folder / "height.png"))[::-1]
                / 65535
                * metadata["height_scale_m"]
            )
            np.testing.assert_allclose(
                height, fields["height_m"], atol=0.012 / 65535 / 2 + 1e-9
            )
            packed = np.asarray(Image.open(folder / "normal_roughness.png"))[::-1]
            np.testing.assert_allclose(
                packed[:, :, 3] / 255, fields["roughness"], atol=0.5 / 255 + 1e-7
            )
            normal = packed[:, :, :3] / 255 * 2 - 1
            normal /= np.linalg.norm(normal, axis=2)[:, :, None]
            dot = np.sum(normal * fields["normal"], axis=2)
            self.assertGreater(float(dot.min()), math.cos(math.radians(0.4)))
        for name in ("colour_linear", "roughness"):
            self.assertTrue(np.all((fields[name] >= 0) & (fields[name] <= 1)))

    def test_source_seed_replay_and_preflight_before_plan_or_pixels(self):
        recipe = FurrowRecipe()
        a, _ = raster(recipe, (32, 32))
        b, _ = raster(recipe, (32, 32))
        np.testing.assert_array_equal(a, b)
        c, _ = raster(replace(recipe, seed=1), (32, 32))
        self.assertFalse(np.array_equal(a, c))
        with patch.object(FurrowRecipe, "plan", side_effect=AssertionError):
            for shape in ((0, 32), (5000, 32), (4096, 4096), (True, 32)):
                with self.assertRaises(ValueError):
                    raster(recipe, shape)
        for kwargs in (
            {"seed": -1},
            {"furrows": 100},
            {"height_m": float("nan")},
            {"width_m": 0.001},
        ):
            with self.assertRaises(ValueError):
                FurrowRecipe(**kwargs)


if __name__ == "__main__":
    unittest.main()
