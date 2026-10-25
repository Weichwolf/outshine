"""Analytic sampling, tangent footprints and immutable per-surface lighting controls."""

import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from building_mass_materials import policy
from building_surface_samples import emit_samples, first_surfaces
from building_surfel_image import footprint_covariance, render, validate
from source_surface_patches import SurfacePatch


def plane(z, normal=(0, 0, 1), *, hole=False):
    domain = shapely.box(0, 0, 4, 4)
    if hole:
        domain = shapely.difference(domain, shapely.box(1, 1, 3, 3))
    axes = np.eye(3)[:2]
    geometry = SurfacePatch(
        domain, np.array([0, 0, z]), axes, np.array(normal, float), 1
    )
    return SimpleNamespace(
        geometry=geometry,
        local_origin=geometry.origin,
        local_axes=axes,
        orientation=1,
        side=1,
        shape={"roof_kind": 0, "ident": 0},
        surface={"wall_colour": None},
    )


class BuildingSurfels(unittest.TestCase):
    def test_fixed_axis_sampling_keeps_both_extremes_and_holes(self):
        sampled, spacing, work = first_surfaces(
            [plane(1), plane(3)], np.array([[0, 0, 0], [4, 4, 4]])
        )
        self.assertEqual(spacing, 0.25)
        self.assertEqual(len(sampled["positions"]), 512)
        np.testing.assert_array_equal(np.unique(sampled["positions"][:, 2]), [1, 3])
        self.assertEqual(work["parallel_patch_axis_pairs"], 4)
        sampled, spacing, work = first_surfaces(
            [plane(1, hole=True)], np.array([[0, 0, 0], [4, 4, 4]])
        )
        self.assertEqual(len(sampled["positions"]), 384)
        self.assertFalse(
            np.any(
                np.all(
                    (sampled["positions"][:, :2] > 1)
                    & (sampled["positions"][:, :2] < 3),
                    axis=1,
                )
            )
        )
        emitted, report = emit_samples(
            [plane(1, hole=True)], sampled, spacing, policy()
        )
        self.assertEqual(report["duplicate_records_removed"], 192)
        self.assertEqual(len(emitted["positions"]), 192)
        np.testing.assert_array_equal(emitted["normal"], np.tile([0, 0, 1], (192, 1)))
        validate(emitted)

    def test_tangent_covariance_matches_front_parallel_projection(self):
        surfels = {
            "positions": np.array([[0, 0, 10.0]]),
            "normal": np.array([[0, 0, -1.0]]),
            "spacing": np.array(1.0),
        }
        covariance, normal = footprint_covariance(
            surfels, (np.zeros(3), np.eye(3), 10, 1, 1)
        )
        np.testing.assert_allclose(covariance, np.eye(2)[None], atol=0, rtol=0)
        np.testing.assert_array_equal(normal, surfels["normal"])

    def test_lighting_receives_individual_normals_and_clear_background_is_paid(self):
        normals = np.array([[0.5, 0, -np.sqrt(0.75)], [-0.5, 0, -np.sqrt(0.75)]])
        surfels = {
            "positions": np.array([[0, 0, 2.0]] * 2),
            "normal": normals,
            "spacing": np.array(0.5),
            "albedo": np.ones((2, 3)),
            "roughness_weights": np.ones((2, 1)),
            "roughness_palette": np.array([0.7]),
        }
        seen = []

        def lighting(albedo, weights, palette, normal, outgoing, light, tables):
            seen.append(normal.copy())
            return np.ones_like(albedo), {"ggx_lobe_evaluations": len(albedo)}

        with patch("building_surfel_image.shade", lighting):
            image, work = render(
                surfels,
                {"indices": np.empty((0, 3), int)},
                {},
                (np.zeros(3), np.eye(3), 2, 1, 1),
                np.array([[0, 0], [1, 1]]),
                {},
                rate=4,
            )
        self.assertEqual(len(seen), 1)
        np.testing.assert_array_equal(seen[0], normals)
        np.testing.assert_allclose(image["C"] + image["T"], 1, atol=1e-15)
        self.assertEqual(work["visible_surface_samples_shaded"], 2)
        with self.assertRaises(ValueError):
            validate(surfels | {"roughness_weights": np.full((2, 1), 0.9)})


if __name__ == "__main__":
    unittest.main()
