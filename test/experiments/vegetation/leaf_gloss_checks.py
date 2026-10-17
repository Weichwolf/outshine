"""Analytic lobe, hemispherical integration, reciprocity and legacy-zero controls."""

import unittest
from unittest.mock import patch

import numpy as np
from leaf_gloss import LeafGloss, specular_response
from leaf_gloss_energy import direct_energy, hemisphere, specular_energy
from outshine_ref.vegetation.leaf_lighting import leaf_diffuse_response
from relief_shading_reference import specular
from scatter_scene import ScatterScene
from scipy.integrate import quad
from sheet_area_checks import thin_sheet
from sheet_leaf_render import shading


class LeafGlossTests(unittest.TestCase):
    def test_zero_fraction_is_exact_sheet_and_does_not_start_specular_work(self):
        optical = np.array([[0.08, 0.22, 0.025, 0.035, 0.12, 0.018]])
        normals = np.array([[0.0, 0, 1]])
        for light in ([0.0, 0, 1], [0.0, 0, -1]):
            expected = leaf_diffuse_response(optical, normals, [0, 0, 1], light)
            with patch(
                "leaf_gloss.specular_response", side_effect=AssertionError("GGX")
            ):
                np.testing.assert_array_equal(
                    LeafGloss(mr_fraction=0)(optical, normals, [0, 0, 1], light),
                    expected,
                )

    def test_normal_incidence_lobe_matches_analytic_value_and_native_reference(self):
        normals = np.array([[0.0, 0, 1]])
        view = np.array([0.0, 0, 1])
        actual = specular_response(normals, 0.5, view, view)
        self.assertAlmostEqual(actual[0], 0.04 / (4 * np.pi * 0.5**4), places=14)
        np.testing.assert_allclose(
            actual, specular(normals, 0.5, view.copy(), view.copy()), atol=1e-14
        )
        np.testing.assert_array_equal(specular_response(normals, 0.5, view, -view), 0)

    def test_reciprocity_and_rigid_rotation_keep_the_same_brdf(self):
        optical = np.array([[0.08, 0.22, 0.025, 0.035, 0.12, 0.018]])
        normal = np.array([[0.0, 0, 1]])
        v = np.array([0.3, 0.4, np.sqrt(0.75)])
        rotation = np.array([[0, 0, 1.0], [1, 0, 0], [0, 1, 0]])
        for light in (
            np.array([0.4, -0.2, np.sqrt(0.8)]),
            np.array([-0.2, 0.3, -np.sqrt(0.87)]),
        ):
            for mode in ("area", "additive"):
                model = LeafGloss(mode=mode)
                value = model(optical, normal, v, light)
                swapped = model(optical, normal, light, v)
                np.testing.assert_allclose(
                    value / abs(light[2]), swapped / abs(v[2]), atol=1e-14
                )
                np.testing.assert_allclose(
                    model(optical, normal @ rotation.T, rotation @ v, rotation @ light),
                    value,
                    atol=1e-14,
                )

    def test_r1_normal_white_furnace_matches_independent_1d_integral(self):
        directions, weights = hemisphere(64)
        actual = specular_energy(1.0, 1.0, directions, weights)
        expected = quad(
            lambda mu: (0.04 + 0.96 * (1 - np.sqrt((1 + mu) / 2)) ** 5) * mu / (1 + mu),
            0,
            1,
            epsabs=1e-14,
        )[0]
        self.assertAlmostEqual(actual, expected, places=13)
        area = direct_energy([0.5] * 6, LeafGloss(1.0, 0.12, "area"), 1.0)
        additive = direct_energy([0.5] * 6, LeafGloss(1.0, 0.12, "additive"), 1.0)
        np.testing.assert_allclose(area, 0.88 + 0.12 * expected, atol=1e-14)
        self.assertTrue((area < 1).all())
        self.assertTrue((additive > 1.001).all())

    def test_area_weights_have_declared_transmission_endpoints(self):
        optical = np.array([[0.08, 0.22, 0.025, 0.035, 0.12, 0.018]])
        normal = np.array([[0.0, 0, 1]])
        for fraction in (0.0, 0.12, 1.0):
            area = LeafGloss(0.3, fraction, "area")
            additive = LeafGloss(0.3, fraction, "additive")
            transmitted = optical[:, 3:] / np.pi
            np.testing.assert_allclose(
                area(optical, normal, [0, 0, 1], [0, 0, -1]),
                (1 - fraction) * transmitted,
                atol=1e-14,
            )
            np.testing.assert_allclose(
                additive(optical, normal, [0, 0, 1], [0, 0, -1]),
                transmitted,
                atol=1e-14,
            )

    def test_shared_primary_renderer_zero_fraction_preserves_all_old_values(self):
        source = thin_sheet()
        scene = ScatterScene(source["vertices"], source["faces"])
        args = (
            source,
            scene,
            np.array([[0.25, 0.25, 0]]),
            np.array([0]),
            np.array([0.0, 0, 1]),
            np.array([[0.0, 0, 1], [0, 0, -1]]),
        )
        original = shading(*args)
        modified = shading(*args, closure=LeafGloss(mr_fraction=0))
        for a, b in zip(original, modified, strict=True):
            np.testing.assert_array_equal(a, b)

    def test_invalid_or_delta_parameters_refuse_before_material_evaluation(self):
        for roughness, fraction, mode in [
            (0, 0.12, "area"),
            (np.nan, 0.12, "area"),
            (0.3, -0.1, "area"),
            (0.3, 0.12, "unknown"),
        ]:
            with self.assertRaises(ValueError):
                LeafGloss(roughness, fraction, mode)


if __name__ == "__main__":
    unittest.main()
