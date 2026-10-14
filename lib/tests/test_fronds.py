"""Independent frond curl geometry, anchors, Jacobians, normals and refusal domains."""

import json
import unittest
from decimal import Decimal
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import FrondCurl, FrondGuide, PinnatePattern


class FrondTests(unittest.TestCase):
    def test_circle_reference_midline_and_neutral_small_curvature_are_correct(self):
        points = np.array([[-0.2, 0.3, 0.01], [0.1, 0.8, -0.02], [0, 0.5, 0]])
        for k in (-3.0, 3.0):
            curl = FrondCurl(k)
            theta = k * points[:, 0]
            expected = np.column_stack(
                (
                    (1 / k - points[:, 2]) * np.sin(theta),
                    points[:, 1],
                    1 / k - (1 / k - points[:, 2]) * np.cos(theta),
                )
            )
            np.testing.assert_allclose(curl.positions(points), expected, atol=2e-16)
            np.testing.assert_array_equal(curl.positions(points)[:, 1], points[:, 1])
            np.testing.assert_array_equal(curl.positions(points)[-1], points[-1])
            np.testing.assert_allclose(
                np.linalg.det(curl.jacobian(points)), 1 - k * points[:, 2], atol=4e-16
            )
        np.testing.assert_array_equal(FrondCurl().positions(points), points)
        tiny = FrondCurl(1e-300).positions([[1e200, 0.5, 0]])
        expected = float(Decimal("1e-300") * Decimal("1e200") ** 2 / 2)
        self.assertTrue(np.isfinite(tiny).all())
        self.assertAlmostEqual(tiny[0, 2] / expected, 1)

    def test_analytic_jacobian_matches_independent_differences(self):
        curl = FrondCurl(3)
        rng = np.random.default_rng(417)
        p = rng.uniform([-0.3, 0, -0.02], [0.3, 1, 0.02], (41, 3))
        jacobian = curl.jacobian(p)
        for axis in range(3):
            step = np.eye(3)[axis] * 1e-6
            reference = (curl.positions(p + step) - curl.positions(p - step)) / 2e-6
            np.testing.assert_allclose(
                jacobian[:, :, axis], reference, atol=7e-11, rtol=0
            )

    def test_mesh_normal_methods_keep_inputs_ids_uvs_and_owned_outputs(self):
        source = PinnatePattern(pairs=4).mesh(24, 2)
        saved = {k: v.copy() for k, v in source.items()}
        curl = FrondCurl(3.5)
        a, b = curl.deform(source), curl.deform(source, "solve")
        for key in a:
            if key == "normals":
                np.testing.assert_allclose(a[key], b[key], atol=7e-16, rtol=0)
            else:
                np.testing.assert_array_equal(a[key], b[key])
            self.assertFalse(np.shares_memory(a[key], source[key]))
        for key, value in saved.items():
            np.testing.assert_array_equal(source[key], value)
        for key in ("faces", "parameters", "organ_ids"):
            np.testing.assert_array_equal(a[key], source[key])
        indices = source["faces"].ravel()
        jacobian = curl.jacobian(source["vertices"])[indices]
        tangent = np.tile([1.0, 0, 0], (len(indices), 1))
        mapped = np.einsum("nij,nj->ni", jacobian, tangent)
        np.testing.assert_allclose(np.sum(mapped * a["normals"], axis=1), 0, atol=7e-16)

    def test_nested_points_and_guide_composition_preserve_source_anchors(self):
        pattern = PinnatePattern(pairs=4)
        fine, coarse = pattern.mesh(48, 4), pattern.mesh(12, 2)
        curl = FrondCurl(3)
        common = (
            fine["vertices"]
            .reshape(9, -1, 3)[:, :-1]
            .reshape(9, 48, 9, 3)[:, ::4, ::2]
            .reshape(-1, 3)
        )
        targets = coarse["vertices"].reshape(9, -1, 3)[:, :-1].reshape(-1, 3)
        np.testing.assert_array_equal(curl.positions(common), curl.positions(targets))
        guide = FrondGuide(bend=-0.5, fold_per_m=0.4)
        result = guide.deform(curl.deform(fine))
        mask = (fine["vertices"][:, 0] == 0) & (fine["vertices"][:, 2] == 0)
        np.testing.assert_array_equal(
            result["vertices"][mask],
            guide.evaluate(fine["vertices"][mask])[0],
        )

    def test_recipe_domain_aliasing_and_budget_counterexamples_refuse(self):
        curl = FrondCurl(3)
        loaded = FrondCurl.from_recipe(json.loads(json.dumps(curl.recipe())))
        np.testing.assert_array_equal(
            curl.positions([[0.1, 0.3, 0]]), loaded.positions([[0.1, 0.3, 0]])
        )
        for points in ([[np.pi / 3, 0, 0]], [[0, 0, 1 / 3]], [[np.nan, 0, 0]]):
            with self.assertRaises(ValueError):
                curl.positions(points)
        for value in (True, np.inf):
            with self.assertRaises(ValueError):
                FrondCurl(value)
        source = {
            "vertices": np.array([[-0.3, 0, 0], [0.3, 0, 0], [0, 0.3, 0]]),
            "faces": np.array([[0, 1, 2]], dtype=np.int32),
            "normals": np.tile([0.0, 0, 1], (3, 1)),
        }
        with self.assertRaises(ValueError):
            FrondCurl(5).deform(source)

        class Oversized:
            def __len__(self):
                return 1000001

        with (
            patch.object(FrondCurl, "_field", side_effect=AssertionError("geometry")),
            self.assertRaises(ValueError),
        ):
            curl.deform({"vertices": Oversized(), "faces": []})


if __name__ == "__main__":
    unittest.main()
