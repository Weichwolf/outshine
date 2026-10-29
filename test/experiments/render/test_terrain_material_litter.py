"""Captured palette and controlled footprint checks for original-word composition."""

import hashlib
import json
import unittest
from pathlib import Path
from unittest.mock import Mock

import numpy as np
from ground_detail import noise2
from ground_litter_footprint import gauss
from ground_litter_reference import detail as reference_detail
from terrain_material_litter import compose, footprint_field, wearing
from terrain_material_plane import hash_words, prepare

ROOT = Path(__file__).resolve().parents[3]


def reference_mask(at, dx, dy, wavelength):
    footprint = max(np.linalg.norm(dx), np.linalg.norm(dy)) / wavelength
    if footprint <= 0.01:
        return noise2(at / wavelength) - 0.5
    return gauss(at / wavelength, dx / wavelength, dy / wavelength, 3)


def word_sources(at, recipe):
    getters = []
    for wavelength in (recipe[2], recipe[3], recipe[0]):
        if wavelength <= 0:
            getters.append(hash_words)
        else:
            normalized = at / wavelength
            getters.append(
                prepare([normalized.min(axis=0), normalized.max(axis=0)]).get
            )
    return getters


class LitterFields(unittest.TestCase):
    def test_oriented_gauss_filter_matches_previous_and_original_lookup(self):
        points = np.random.default_rng(1684).uniform(-2, 2, (32, 2))
        plane = prepare([[-2, -2], [2, 2]], halo=3)
        for width in (0, 0.005, 0.2, 1.2, 1.99, 2, 4):
            dx, dy = (
                np.tile([width, 0], (32, 1)),
                np.tile([0.6 * width, 0.8 * width], (32, 1)),
            )
            direct = footprint_field(points, dx, dy, 1)
            lookup = footprint_field(points, dx, dy, 1, words=plane.get)
            np.testing.assert_array_equal(direct, lookup)
            old = reference_mask(points, dx[0], dy[0], 1)
            np.testing.assert_allclose(direct, old, rtol=0, atol=1e-15)

    def test_captured_native_palette_rows_keep_full_composition(self):
        folder = ROOT / "test/outshine/data/materials"
        manifest = json.loads((folder / "ground-litter.json").read_bytes())
        raw = (folder / "ground-litter.palette").read_bytes()
        self.assertEqual(hashlib.sha256(raw).hexdigest(), manifest["sha256"])
        palette = np.frombuffer(raw, dtype="<f4")
        rows, layers = manifest["rows"], manifest["layer_offset_floats"]
        surfaces = 4 * (rows + 2) + rows + 1
        points = np.random.default_rng(5).uniform(0, 0.01, (16, 2)) + [1000.1, -200.2]
        for row in range(rows):
            recipe = palette[surfaces + row * 8 : surfaces + row * 8 + 4].astype(float)
            contrast, coverage = (
                palette[surfaces + row * 8 + 4],
                float(palette[surfaces + row * 8 + 7]),
            )
            mean = palette[4 + row * 4 : 8 + row * 4].astype(float)
            delta = palette[layers + row * 8 : layers + row * 8 + 4].astype(float)
            litter = palette[layers + row * 8 + 4 : layers + row * 8 + 8].astype(float)
            base_sources, litter_sources = (
                word_sources(points, recipe),
                word_sources(points, litter),
            )
            mask = prepare(
                [points.min(axis=0) / recipe[2], points.max(axis=0) / recipe[2]], halo=3
            )
            for footprint in (0.0001, 0.01, 1.0):
                dx, dy = (
                    np.tile([footprint, 0.2 * footprint], (16, 1)),
                    np.tile([-0.1 * footprint, footprint], (16, 1)),
                )
                direct = compose(
                    points, dx, dy, mean, recipe, contrast, litter, delta, coverage
                )
                lookup = compose(
                    points,
                    dx,
                    dy,
                    mean,
                    recipe,
                    contrast,
                    litter,
                    delta,
                    coverage,
                    base_words=base_sources,
                    litter_words=litter_sources,
                    mask_words=mask.get,
                )
                np.testing.assert_array_equal(direct, lookup)
                field = reference_mask(points, dx[0], dy[0], recipe[2])
                weight = np.clip(
                    coverage + 2 * min(coverage, 1 - coverage) * field, 0, 1
                )
                base = reference_detail(points, dx, dy, recipe, contrast)
                top = reference_detail(points, dx, dy, litter, contrast)
                first = mean - coverage * delta
                old = (
                    wearing(first, base) * (1 - weight[:, None])
                    + wearing(first + delta, top) * weight[:, None]
                )
                with self.subTest(row=row, footprint=footprint):
                    np.testing.assert_allclose(direct, old, rtol=0, atol=3e-11)

    def test_endpoint_coverage_and_far_filter_do_no_unused_work(self):
        at, dx, dy = [[0.1, 0.2]], [[0.1, 0]], [[0, 0.1]]
        recipe, mean, delta = (
            [0.01, 0.004, 1.6, 0.239],
            [0.1, 0.2, 0.3, 0.7],
            [0.01, 0.02, -0.01, 0.1],
        )
        forbidden = Mock(side_effect=AssertionError("unused layer work"))
        compose(
            at,
            dx,
            dy,
            mean,
            recipe,
            0.5,
            recipe,
            delta,
            0,
            litter_words=(forbidden,) * 3,
            mask_words=forbidden,
        )
        compose(
            at,
            dx,
            dy,
            mean,
            recipe,
            0.5,
            recipe,
            delta,
            1,
            base_words=(forbidden,) * 3,
            mask_words=forbidden,
        )
        np.testing.assert_array_equal(
            footprint_field(at, [[2, 0]], [[0, 2]], 1, words=forbidden), [0]
        )
        forbidden.assert_not_called()


if __name__ == "__main__":
    unittest.main()
