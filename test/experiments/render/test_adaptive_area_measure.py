"""Independent mass, overlap, tiny-feature, GGX and metadata rejection controls."""

import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
import shapely
from adaptive_area_measure import compile_measure, leaf_bounds
from area_measure_bake import components
from area_measure_render import evaluate, program
from bench_area_measure import load_product
from opaque_area_reference import evaluate as reference_evaluate
from opaque_area_reference import pixel_grid, quadrature
from packed_normals import pack
from visible_normal_material import tables


class MeasureTests(unittest.TestCase):
    def test_constant_measure_needs_no_refinement(self):
        word = pack([[0, 0, -1]])
        arrays, work = compile_measure(
            [shapely.box(0, 0, 1, 1)], word, max_array_bytes=10000
        )
        self.assertEqual(work["nodes"], 1)
        np.testing.assert_array_equal(arrays["weights"], [1])
        self.assertEqual(work["remaining_l1_measure_error"], 0)

    def test_partition_refines_and_preserves_each_normal_mass(self):
        words = pack([[0, 0, -1], [1, 0, 0]])
        polygons = [shapely.box(0, 0, 0.5, 1), shapely.box(0.5, 0, 1, 1)]
        arrays, work = compile_measure(polygons, words, max_array_bytes=108)
        self.assertEqual(work["nodes"], 5)
        self.assertEqual(work["stored_array_bytes"], 108)
        self.assertEqual(work["remaining_l1_measure_error"], 0)
        mass = {}
        for leaf, bounds in enumerate(leaf_bounds(arrays)):
            area = np.prod(bounds[1] - bounds[0])
            a, b = arrays["offsets"][leaf : leaf + 2]
            for word, weight in zip(
                arrays["normal_words"][a:b], arrays["weights"][a:b], strict=True
            ):
                mass[int(word)] = mass.get(int(word), 0) + area * weight
        np.testing.assert_allclose(list(mass.values()), [0.5, 0.5], atol=1e-15)

    def test_positive_tiny_triangle_is_not_missed_by_point_training(self):
        triangle = shapely.Polygon([(0.1, 0.1), (0.10001, 0.1), (0.1, 0.10001)])
        arrays, _ = compile_measure([triangle], pack([[0, 0, -1]]), max_array_bytes=28)
        self.assertGreater(arrays["weights"][0], 0)
        self.assertAlmostEqual(arrays["weights"][0], triangle.area, places=18)

    def test_overlapping_input_fails_instead_of_normalizing_mass(self):
        with self.assertRaisesRegex(ValueError, "non-disjoint"):
            compile_measure(
                [shapely.box(0, 0, 1, 1)] * 2,
                pack([[0, 0, -1], [1, 0, 0]]),
                max_array_bytes=1000,
            )

    def test_conditioning_guard_is_per_component_and_reports_removed_mass(self):
        tiny = shapely.Polygon([(0, 0), (1e-7, 0), (0, 1e-7)])
        whole = shapely.GeometryCollection([shapely.box(0.1, 0.1, 0.2, 0.2), tiny])
        polygons, faces, work = components([(17, whole)], 1)
        self.assertEqual(len(polygons), 1)
        self.assertEqual(faces, [17])
        self.assertAlmostEqual(
            work["conditioning_excluded_reported_area_px"], tiny.area
        )
        self.assertEqual(work["conditioning_affected_faces"], [17])

    def test_homogeneous_quad_matches_independent_analytic_reference(self):
        normal = np.array([0.0, 0.0, -1.0])
        arrays, _ = compile_measure(
            [shapely.box(0, 0, 1, 1)], pack([normal]), max_array_bytes=100
        )
        geometry = {
            "eye": np.zeros(3),
            "basis": np.eye(3),
            "start": np.array([-0.01, -0.01]),
            "step": np.full(2, 0.02),
            "plane": 2.0,
        }
        view = (np.zeros(3), np.diag([1.0, -1.0, 1.0]), 100.0, 4, 4)
        box = np.array([[0, 0], [4, 4]])
        p = program(arrays, geometry, view, box, refined=True)
        image, coverage = evaluate(p, [4, 4], normal, 0.45, tables())
        source = {
            "positions": np.array(
                [
                    [-0.02, -0.02, 2],
                    [0.02, -0.02, 2],
                    [0.02, 0.02, 2],
                    [-0.02, 0.02, 2],
                ],
                float,
            ),
            "indices": np.array([[0, 3, 2], [0, 2, 1]]),
            "vertex_normal": np.tile(normal, (4, 1)),
        }
        reference = quadrature(
            list(pixel_grid(source, view, box).fragments()), view, box, refined=True
        )
        expected, expected_coverage = reference_evaluate(
            reference, source, [4, 4], normal, 0.45, tables()
        )
        np.testing.assert_allclose(coverage, expected_coverage, atol=1e-14)
        np.testing.assert_allclose(image, expected, atol=2e-6)
        self.assertAlmostEqual(float(coverage.sum()), 4.0, places=12)

    def test_identity_and_observer_domain_checked_before_payload(self):
        with tempfile.TemporaryDirectory() as tmp:
            header = {
                "format": "adaptive-opaque-normal-measure-1",
                "provenance": {},
                "observer_bounds": [[0, 0, 0], [1, 1, 1]],
            }
            Path(tmp, "header.json").write_text(json.dumps(header))
            with self.assertRaisesRegex(ValueError, "identity"):
                load_product(tmp, {"changed": 1}, [[0, 0, 0]])
            with self.assertRaisesRegex(ValueError, "outside"):
                load_product(tmp, {}, [[2, 0, 0]])


if __name__ == "__main__":
    unittest.main()
