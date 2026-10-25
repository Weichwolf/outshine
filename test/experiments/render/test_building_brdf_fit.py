"""Sparse fits preserve their physical simplex and the renderer's diffuse model."""

import hashlib
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from building_brdf_basis import angular_samples, response_basis
from building_brdf_fit import emitted_error, fit_weights, interval_pair
from building_mass_lighting import shade
from fit_building_brdf import read_fit, read_material


class BuildingBrdfFit(unittest.TestCase):
    def test_known_three_axis_minimax_problem_and_two_component_identity(self):
        basis = {"specular": np.eye(3), "diffuse": np.ones(3)}
        weights = np.full(3, 1 / 3, np.float32)
        for budget, expected in ((1, 0.5), (2, 0.25)):
            fitted, work = fit_weights(weights, np.ones(3), basis, max_lobes=budget)
            self.assertLessEqual(np.count_nonzero(fitted), budget)
            self.assertTrue((fitted >= 0).all())
            self.assertAlmostEqual(float(fitted.sum()), 1)
            self.assertAlmostEqual(work["training_relative_error"], expected, places=6)
        exact = np.array([0.2, 0, 0.8], np.float32)
        fitted, work = fit_weights(exact, np.ones(3), basis)
        self.assertEqual(fitted.tobytes(), exact.tobytes())
        self.assertEqual(work["pairs"], 0)
        self.assertTrue(work["already_within_budget"])

    def test_interval_constant_constraints_and_invalid_material(self):
        weight, _ = interval_pair(np.array([0, 1.0]), np.array([0.25, 0.5]))
        self.assertLessEqual(max(0.25, abs(weight - 0.5)), 0.25 + 1e-9)
        with self.assertRaises(ValueError):
            fit_weights(
                [-0.1, 0.6, 0.5],
                [1, 1, 1],
                {"specular": np.eye(3), "diffuse": np.ones(3)},
            )

    def test_nearly_equal_response_planes_need_no_reciprocal_tolerance(self):
        delta = np.array([1.0, np.nextafter(1.0, 2.0)])
        target = np.full(2, 0.3)
        weight, _ = interval_pair(delta, target)
        self.assertTrue(0 <= weight <= 1)
        self.assertLess(float(abs(delta * weight - target).max()), 1e-12)

    def test_material_reader_never_decodes_geometry_or_source_lineage(self):
        material = {
            "m__roughness_weights": np.array([[[0.2, 0.8], [0.4, 0.6]]], np.float32),
            "m__albedo": np.full((1, 2, 3), 0.5, np.float32),
            "m__roughness_palette": np.array([0.12, 0.88], np.float32),
            "m__supported": np.ones(1, bool),
        }
        with tempfile.TemporaryDirectory(prefix="outshine-brdf-material-") as temporary:
            path = Path(temporary) / "node.npz"
            np.savez(
                path,
                **material,
                g__positions=np.zeros((3, 3)),
                g__group_sources=[1, 2, 3],
            )
            with np.load(path, allow_pickle=False) as archive:
                cls = type(archive)
            original = cls.__getitem__

            def getitem(archive, key):
                self.assertTrue(key.startswith("m__"))
                return original(archive, key)

            with patch.object(cls, "__getitem__", getitem):
                result = read_material(path)
            self.assertEqual(set(result), set(material))
            for key, value in material.items():
                np.testing.assert_array_equal(result[key], value)
            fitted_path = Path(temporary) / "fit.npz"
            digest = hashlib.sha256(path.read_bytes()).digest()
            np.savez(
                fitted_path,
                weights=material["m__roughness_weights"],
                source_node_sha256=np.frombuffer(digest, np.uint8),
            )
            with patch(
                "fit_building_brdf.fit_weights",
                side_effect=AssertionError("loading must not fit"),
            ):
                actual = read_fit(fitted_path, path, (1, 2, 2), 2)
            np.testing.assert_array_equal(actual, material["m__roughness_weights"])
            with self.assertRaisesRegex(ValueError, "palette budget"):
                read_fit(fitted_path, path, (1, 2, 2), 1)
            path.write_bytes(b"different immutable source node")
            with self.assertRaisesRegex(ValueError, "different original node"):
                read_fit(fitted_path, path, (1, 2, 2), 2)

    def test_basis_matches_current_one_hot_renderer_for_varied_lights(self):
        with np.load(
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/material-filter/energy-tables.npz",
            allow_pickle=False,
        ) as archive:
            tables = dict(archive)
        palette = np.array([0.12, 0.72, 0.91], np.float32)
        samples = np.array(
            [
                [nl, nv, phi]
                for nl in (0.1, 0.7)
                for nv, phi in ((0.2, 0.3), (0.6, 1.7), (0.9, np.pi))
            ]
        )
        basis = response_basis(palette, tables, samples)
        for nl in (0.1, 0.7):
            selected = samples[:, 0] == nl
            _, nv, phi = samples[selected].T
            outgoing = np.c_[
                np.sqrt(1 - nv * nv) * np.cos(phi),
                np.sqrt(1 - nv * nv) * np.sin(phi),
                nv,
            ]
            for component in range(len(palette)):
                weights = np.zeros((len(nv), len(palette)))
                weights[:, component] = 1
                value, _ = shade(
                    np.ones((len(nv), 3)),
                    weights,
                    palette,
                    np.tile([0, 0, 1], (len(nv), 1)),
                    outgoing,
                    [np.sqrt(1 - nl * nl), 0, nl],
                    tables,
                    intensity=1,
                    ambient=0,
                )
                target = (
                    basis["diffuse"][selected] + basis["specular"][selected, component]
                )
                np.testing.assert_allclose(
                    value, np.repeat(target[:, None], 3, axis=1), rtol=1e-12, atol=1e-12
                )
            albedo = np.tile([0.2, 0.4, 0.6], (len(nv), 1))
            normal = np.tile([0, 0, 1], (len(nv), 1))
            source_weights = np.array([0.11, 0.27, 0.62], np.float32)
            fitted_weights = np.array([0.2, 0, 0.8], np.float32)
            values = []
            for measure in (source_weights, fitted_weights):
                image, _ = shade(
                    albedo,
                    np.tile(measure, (len(nv), 1)),
                    palette,
                    normal,
                    outgoing,
                    [np.sqrt(1 - nl * nl), 0, nl],
                    tables,
                    intensity=1,
                    ambient=0,
                )
                values.append(image)
            expected = abs(values[1] - values[0]) / values[0]
            local = {
                "specular": basis["specular"][selected],
                "diffuse": basis["diffuse"][selected],
            }
            np.testing.assert_allclose(
                emitted_error(source_weights, fitted_weights, albedo[0], local),
                expected,
                rtol=1e-11,
                atol=1e-11,
            )
        self.assertFalse(
            np.array_equal(angular_samples(), angular_samples(validation=True))
        )


if __name__ == "__main__":
    unittest.main()
