"""Independent support geometry, local absence, immutable ownership and work bounds."""

import dataclasses
import json
import unittest
from unittest.mock import patch
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.transport.supports import SupportBudget, SupportVolumeBuilder
from outshine_ref.transport.supports._geometry import preflight_noding


def grid(height):
    xy = np.array([[x, y] for y in (-2.0, 0, 2) for x in (-2.0, 0, 2)])
    first = np.array([0, 1, 3, 4])
    faces = np.vstack(
        (np.c_[first, first + 1, first + 4], np.c_[first, first + 4, first + 3])
    )
    return Mesh(np.c_[xy, height(xy)], faces)


def closed_volume(mesh):
    edges = mesh.triangles[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    _, ids, counts = np.unique(
        np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    signs = np.where(edges[:, 0] < edges[:, 1], 1, -1)
    if np.any(counts != 2) or np.any(np.bincount(ids, weights=signs) != 0):
        raise ValueError("not a closed consistently directed body")
    p = mesh.vertices[mesh.triangles] - mesh.vertices[0]
    return float(np.einsum("ij,ij->i", p[:, 0], np.cross(p[:, 1], p[:, 2])).sum() / 6)


class SupportAPI(unittest.TestCase):
    def setUp(self):
        self.lower = grid(lambda p: abs(p[:, 0]))
        self.upper = grid(lambda p: 6 + 0.3 * abs(p[:, 1]))
        self.footprint = shapely.box(-1, -1, 1, 1)

    def build(self, budget=None):
        budget = SupportBudget() if budget is None else budget
        return SupportVolumeBuilder(self.lower, self.upper, budget).build(
            self.footprint, minimum_height_m=2
        )

    def test_analytic_crossing_creases_and_closed_volume(self):
        product, note = self.build()
        self.assertTrue(note["valid"])
        low, high = product.lower_contact.vertices, product.upper_contact.vertices
        np.testing.assert_array_equal(low[:, :2], high[:, :2])
        np.testing.assert_allclose(low[:, 2], abs(low[:, 0]), atol=1e-12)
        np.testing.assert_allclose(high[:, 2], 6 + 0.3 * abs(high[:, 1]), atol=1e-12)
        self.assertAlmostEqual(closed_volume(product.body), 4 * (6 + 0.15 - 0.5))
        self.assertGreater(note["contact_faces"], 2)

    def test_products_and_builder_have_immutable_owned_geometry(self):
        builder = SupportVolumeBuilder(self.lower, self.upper)
        product, _ = builder.build(self.footprint, minimum_height_m=2)
        for mesh in (product.body, product.lower_contact, product.upper_contact):
            for array in (mesh.vertices, mesh.triangles):
                self.assertFalse(array.flags.writeable)
                self.assertFalse(np.shares_memory(array, self.lower.vertices))
                with self.assertRaises(ValueError):
                    array.flat[0] = 3
        with self.assertRaises(dataclasses.FrozenInstanceError):
            builder.lower = self.upper

    def test_interior_peak_has_no_unchecked_partial_product(self):
        low = grid(lambda p: 8 - 4 * abs(p[:, 0]))
        product, note = SupportVolumeBuilder(low, self.upper).build(
            self.footprint, minimum_height_m=2
        )
        self.assertIsNone(product)
        self.assertIn("insufficient whole contact height", note["reason"])

    def test_hole_remains_an_oriented_inner_wall(self):
        footprint = self.footprint.difference(shapely.box(-0.2, -0.2, 0.2, 0.2))
        low = grid(lambda p: np.zeros(len(p)))
        high = grid(lambda p: np.full(len(p), 5.0))
        product, _ = SupportVolumeBuilder(low, high).build(
            footprint, minimum_height_m=2
        )
        self.assertAlmostEqual(closed_volume(product.body), footprint.area * 5)

    def test_missing_finite_contact_is_explicit(self):
        product, note = SupportVolumeBuilder(self.lower, self.upper).build(
            shapely.box(-1, -1, 2.01, 1), minimum_height_m=2
        )
        self.assertIsNone(product)
        self.assertIn("outside selected contact", note["reason"])

    def test_local_ambiguous_stacked_faces_are_not_a_surface_selection(self):
        bad = Mesh(
            np.vstack((self.lower.vertices, self.lower.vertices + [0, 0, 0.2])),
            np.vstack((self.lower.triangles, self.lower.triangles + 9)),
        )
        product, note = SupportVolumeBuilder(bad, self.upper).build(
            self.footprint, minimum_height_m=2
        )
        self.assertIsNone(product)
        self.assertIn("unambiguous", note["reason"])

    def test_bad_region_does_not_invalidate_disjoint_valid_contact(self):
        vertices = np.vstack(
            (
                self.lower.vertices,
                self.lower.vertices + [20, 0, 0],
                self.lower.vertices + [20, 0, 0.2],
            )
        )
        faces = np.vstack(
            (self.lower.triangles, self.lower.triangles + 9, self.lower.triangles + 18)
        )
        product, note = SupportVolumeBuilder(Mesh(vertices, faces), self.upper).build(
            self.footprint, minimum_height_m=2
        )
        self.assertIsNotNone(product)
        self.assertTrue(note["valid"])

    def test_work_and_time_budgets_return_reasons(self):
        for budget, reason in (
            (SupportBudget(maximum_patch_faces=1), "face budget"),
            (SupportBudget(maximum_segments=1), "segment budget"),
            (SupportBudget(maximum_contact_faces=1), "face budget"),
            (SupportBudget(maximum_interface_candidates=1), "candidate budget"),
            (SupportBudget(maximum_sample_candidates=1), "candidate_budget"),
            (SupportBudget(maximum_footprint_coordinates=4), "coordinate budget"),
            (SupportBudget(seconds=1e-20), "time budget"),
        ):
            with self.subTest(budget=budget):
                product, note = self.build(budget)
                self.assertIsNone(product)
                self.assertIn(reason, note["reason"])

    def test_crossing_segment_work_is_bounded_before_arrangement_emission(self):
        lines = shapely.linestrings(
            [[[x, -10], [x, 10]] for x in range(-9, 10)]
            + [[[-10, y], [10, y]] for y in range(-9, 10)]
        )
        with patch.object(
            shapely, "union_all", side_effect=AssertionError("noding must not run")
        ):
            with self.assertRaisesRegex(ValueError, "preflight segment budget"):
                preflight_noding(lines, 100)

    def test_invalid_recipe_and_budget_are_errors(self):
        for options in (
            {"maximum_contact_faces": 0},
            {"maximum_segments": True},
            {"maximum_surface_faces": 262145},
            {"seconds": float("inf")},
            {"seconds": -1},
        ):
            with self.assertRaises(ValueError):
                SupportBudget(**options)
        builder = SupportVolumeBuilder(self.lower, self.upper)
        for shape, height in (
            (self.footprint, 0),
            (self.footprint, float("nan")),
            (shapely.Polygon(), 2),
            (shapely.Polygon([(0, 0, 1), (1, 0, 1), (0, 1, 1)]), 2),
        ):
            with self.assertRaises(ValueError):
                builder.build(shape, minimum_height_m=height)

    def test_vertical_or_degenerate_input_is_an_error(self):
        bad = Mesh(np.array([[0, 0, 0], [0, 0, 1], [0, 1, 1]]), np.array([[0, 1, 2]]))
        with self.assertRaises(ValueError):
            SupportVolumeBuilder(bad, self.upper)

    def test_real_vienna_pinned_patch_closes_without_precision_reduction(self):
        path = Path(__file__).parent / "fixtures/bridge-support-contact-vienna.json"
        row = json.loads(path.read_text())
        lower = Mesh(np.array(row["lower_vertices"]), np.array(row["lower_faces"]))
        upper = Mesh(np.array(row["upper_vertices"]), np.array(row["upper_faces"]))
        product, note = SupportVolumeBuilder(lower, upper).build(
            shapely.Polygon(row["footprint"]), minimum_height_m=2
        )
        self.assertTrue(note["valid"])
        self.assertGreater(closed_volume(product.body), 0)
        self.assertLess(note["coverage_error_m2"], 1e-8)


if __name__ == "__main__":
    unittest.main()
