"""Independent geometry controls and retained original-cell boundary regressions."""

import copy
import unittest
from pathlib import Path
from types import SimpleNamespace

import numpy as np
from attributed_prisms import (
    compile_product,
    load_product,
    native_inputs,
    render_product,
)
from fit_prism_planes import (
    fit_product,
    fitted_planes,
    geometry_arrays,
    render_source_shading,
)
from full_view_reference import load, views
from native_capture_meshes import independent_depth
from packed_normals import pack, unpack
from prepared_surface_recipes import load_recipes
from primitive_bvh import build, trace
from roof_prism_control import explicit_mesh
from roof_prisms import Prisms
from test_roof_prisms import box
from triangle_image_reference import image_rays


def affine_cube():
    equations = box()
    transform = np.array([[1.0, 0.003, 0.0], [0.0, 1.0, -0.002], [0.001, 0.0, 1.0]])
    shift = np.array([0.0003, -0.0002, 0.0001])
    original = explicit_mesh(equations, np.zeros(3))
    positions = (original @ transform.T + shift).reshape(-1, 3)
    source = {
        "positions": positions,
        "indices": np.arange(len(positions)).reshape(-1, 3),
        "binding": np.tile([0, 17, -1, -1, 0, -1], (len(original), 1)),
    }
    prisms = Prisms(
        equations,
        np.array([0, 6]),
        np.array([[17, 0]]),
        np.array([[[-1, -1, -1], [1, 1, 1]]], float),
        np.zeros(6, bool),
        np.empty(0, int),
    )
    return source, prisms


class PlaneFitTest(unittest.TestCase):
    def test_fit_matches_independent_affine_native_triangles(self):
        source, prisms = affine_cube()
        originals = copy.deepcopy((source, prisms))
        materials = SimpleNamespace(frames=np.zeros((1, 4, 3)))
        fallback, prepared = {}, object()
        fitted, report = fit_product(
            (prisms, materials, build(prisms.bounds), fallback, prepared), source
        )
        rng = np.random.default_rng(1161)
        origins = rng.normal(size=(256, 3))
        origins *= 4 / np.linalg.norm(origins, axis=1)[:, None]
        directions = rng.uniform(-0.5, 0.5, (256, 3)) - origins
        expected = independent_depth(
            source["positions"][source["indices"]], origins, directions
        )
        actual, _, _, _ = trace(fitted[0], fitted[2], origins, directions)
        np.testing.assert_allclose(actual, expected, atol=1e-10, rtol=1e-10)
        self.assertEqual(report["fitted_planes"], 6)
        self.assertLess(report["fitted_max_assigned_vertex_residual_m"], 1e-12)
        self.assertIs(fitted[1], materials)
        self.assertIs(fitted[3], fallback)
        self.assertIs(fitted[4], prepared)
        np.testing.assert_array_equal(prisms.planes, originals[1].planes)
        for name in source:
            np.testing.assert_array_equal(source[name], originals[0][name])

    def test_unobserved_plane_stays_unchanged(self):
        source, prisms = affine_cube()
        triangles = source["positions"][source["indices"]]
        normal = np.cross(
            triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        )
        keep = normal[:, 2] > -0.5
        source["indices"] = source["indices"][keep]
        source["binding"] = source["binding"][keep]
        planes, report = fitted_planes(prisms, source)
        np.testing.assert_array_equal(planes[5], prisms.planes[5])
        self.assertEqual(report["unchanged_planes"], 1)

    def test_missing_source_and_degenerate_native_geometry_reject(self):
        source, prisms = affine_cube()
        source["binding"][:, 1] = 18
        with self.assertRaisesRegex(ValueError, "no native triangles"):
            fitted_planes(prisms, source)
        source, prisms = affine_cube()
        source["positions"][:] = 0
        with self.assertRaisesRegex(ValueError, "nondegenerate"):
            fitted_planes(prisms, source)

    def test_all_original_fallback_remains_a_shared_product(self):
        prisms = SimpleNamespace(binding=np.empty((0, 2), np.int32))
        product = prisms, object(), object(), object(), object()
        result, _ = fit_product(product, {})
        self.assertIs(result, product)

    def test_serialization_preserves_omitted_bounds_and_fitting_integer_width(self):
        source, prisms = affine_cube()
        fitted, _ = fit_product(
            (
                prisms,
                SimpleNamespace(frames=np.zeros((1, 4, 3))),
                build(prisms.bounds),
                {},
                None,
            ),
            source,
        )
        arrays = {
            "prism_bounds": np.empty((0, 2, 3)),
            "node_children": np.array([[-1, 0]], np.int16),
            "unchanged": np.ones(3),
        }
        words = pack(prisms.planes[:, :3])
        stored = geometry_arrays(arrays, fitted, words)
        self.assertEqual(stored["node_children"].dtype, np.int16)
        self.assertIs(stored["prism_bounds"], arrays["prism_bounds"])
        self.assertIs(stored["unchanged"], arrays["unchanged"])
        self.assertIs(stored["fit_normal_words"], words)


class OriginalBoundaryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import tempfile

        cls.temp = tempfile.TemporaryDirectory(prefix="outshine-tests.prism-plane-fit-")
        cls.addClassCleanup(cls.temp.cleanup)
        path = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-full-view/cell.json.gz"
        )
        cls.source, cls.document, cls.context, _ = load(path)
        recipes, _ = load_recipes(path, cls.document)
        arrays, _ = compile_product(cls.document, cls.source, recipes)
        product_path = Path(cls.temp.name) / "product.npz"
        np.savez_compressed(product_path, **arrays)
        cls.baseline = load_product(product_path)
        cls.fitted, cls.report = fit_product(cls.baseline, cls.source)
        cls.poses = views(cls.document, cls.context)

    def test_both_retained_boundary_rays_match_original_first_surface(self):
        cases = [
            ("moving-023", [[346, 285], [401, 303]], 2553),
            ("static-025", [[155, 285], [223, 304]], 2678),
        ]
        inputs = native_inputs(self.source)
        for name, region, index in cases:
            with self.subTest(pose=name):
                size = np.diff(region, axis=0)[0]
                origins, directions = image_rays(
                    self.poses[name], np.array(region), *size, 2
                )
                origins, directions = (
                    origins[index : index + 1],
                    directions[index : index + 1],
                )
                actual = render_product(self.fitted, origins, directions)
                original_depth = independent_depth(
                    self.source["positions"][self.source["indices"]],
                    origins,
                    directions,
                )
                self.assertTrue(np.isfinite(actual[0][0]))
                self.assertTrue(np.isfinite(original_depth[0]))
                expected_code = 133 if name == "moving-023" else -994
                self.assertEqual(actual[1]["code"][0], expected_code)
                baseline = render_product(self.baseline, origins, directions)
                self.assertTrue(
                    not np.isfinite(baseline[0][0])
                    or baseline[1]["code"][0] != expected_code
                )
        self.assertEqual(self.report["source_groups"], 557)
        self.assertEqual(
            self.report["assigned_triangles"], len(inputs["indices"]) - 887
        )

    def test_source_shading_keeps_original_normals_and_native_fallback(self):
        origins, directions = image_rays(
            self.poses["moving-023"], np.array([[346, 285], [401, 303]]), 55, 18, 2
        )
        words = pack(self.baseline[0].planes[:, :3])
        geometry = render_product(self.fitted, origins, directions)
        shaded = render_source_shading(self.fitted, words, origins, directions)
        analytic = shaded[4] >= 0
        fallback = shaded[6] >= 0
        self.assertTrue(fallback.any())
        np.testing.assert_array_equal(
            shaded[1]["normal"][analytic], unpack(words[shaded[5][analytic]])
        )
        np.testing.assert_array_equal(
            shaded[1]["normal"][fallback], geometry[1]["normal"][fallback]
        )
        for key in geometry[1]:
            if key != "normal":
                np.testing.assert_array_equal(shaded[1][key], geometry[1][key])
        for index in [0, 2, 3, 4, 5, 6]:
            np.testing.assert_array_equal(shaded[index], geometry[index])
        self.assertEqual(shaded[-1]["source_normal_word_queries"], int(analytic.sum()))
