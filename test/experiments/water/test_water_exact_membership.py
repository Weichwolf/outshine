import unittest
from fractions import Fraction
from pathlib import Path

import numpy as np
import shapely
from shapely.geometry import MultiPolygon, Polygon, box
from water_exact_membership import ExactBudget, ExactBudgetExceeded, exact_overlap


class ExactMembership(unittest.TestCase):
    def test_area_membership_and_union_do_not_double_count_overlapping_sources(self):
        triangle = [[0, 0], [4, 0], [0, 4]]
        areas, missing = exact_overlap(
            triangle, [box(-1, -1, 2, 5), box(-1, -1, 5, 2)], ExactBudget()
        )
        self.assertEqual(areas, (Fraction(6), Fraction(6)))
        self.assertEqual(missing, 0)
        areas, missing = exact_overlap(
            triangle, [box(-1, -1, 1, 5), box(-1, -1, 5, 1)], ExactBudget()
        )
        self.assertEqual(areas, (Fraction(7, 2), Fraction(7, 2)))
        self.assertEqual(missing, 2)

    def test_holes_concavity_and_disconnected_parts_use_ring_parity(self):
        triangle = [[0, 0], [8, 0], [0, 8]]
        hole = box(-1, -1, 9, 9).difference(box(1, 1, 2, 2))
        islands = MultiPolygon([box(0, 0, 1, 1), box(2, 0, 3, 1)])
        concave = Polygon([(0, 0), (3, 0), (3, 1), (1, 1), (1, 3), (0, 3)])
        areas, missing = exact_overlap(
            triangle, [hole, islands, concave], ExactBudget()
        )
        self.assertEqual(areas, (Fraction(31), Fraction(2), Fraction(5)))
        self.assertEqual(missing, 1)
        # A source covering the hole changes union coverage independently of the
        # original hole-bearing owner's area; no island or source is discarded.
        areas, missing = exact_overlap(triangle, [hole, box(1, 1, 2, 2)], ExactBudget())
        self.assertEqual(areas, (Fraction(31), Fraction(1)))
        self.assertEqual(missing, 0)

    def test_only_edge_point_or_empty_contact_has_zero_area(self):
        tri = [[0, 0], [2, 0], [0, 2]]
        areas, missing = exact_overlap(
            tri, [box(2, 0, 3, 1), box(-1, -1, 0, 0)], ExactBudget()
        )
        self.assertEqual(areas, (0, 0))
        self.assertEqual(missing, 2)
        self.assertEqual(exact_overlap(tri, [], ExactBudget()), ((), Fraction(2)))

    def test_tiny_positive_slivers_and_reversed_ring_orders_are_retained(self):
        tri = [[0, 0], [2, 0], [0, 2]]
        width = 2.0**-45
        source = box(0, 0, width, 1)
        for t in (tri, tri[::-1]):
            for p in (source, shapely.reverse(source)):
                a, missing = exact_overlap(t, [p], ExactBudget())
                self.assertEqual(a, (Fraction(width),))
                self.assertEqual(missing, Fraction(2) - Fraction(width))

    def test_translation_reflection_and_dyadic_scaling_preserve_exact_areas(self):
        triangle = np.array([[0.0, 0], [4, 0], [0, 4]])
        polygon = box(-1, -1, 2, 5)
        for scale, origin in (
            (1, [2**30, -(2**29)]),
            (-2, [17, -51]),
            (0.125, [-19, 37]),
        ):
            transformed = shapely.transform(
                polygon, lambda xy, scale=scale, origin=origin: xy * scale + origin
            )
            areas, missing = exact_overlap(
                triangle * scale + origin, [transformed], ExactBudget()
            )
            self.assertEqual(areas, (Fraction(6) * Fraction(scale) ** 2,))
            self.assertEqual(missing, Fraction(2) * Fraction(scale) ** 2)

    def test_input_and_arithmetic_caps_refuse_without_a_partial_result(self):
        tri = [[0, 0], [4, 0], [0, 4]]
        for budget in (
            ExactBudget(limit=0),
            ExactBudget(limit=20),
            ExactBudget(max_edges=3),
        ):
            with self.assertRaises(ExactBudgetExceeded):
                exact_overlap(tri, [box(0, 0, 2, 2)], budget)
            self.assertLessEqual(budget.operations, budget.limit)
        with self.assertRaises(ExactBudgetExceeded):
            exact_overlap([[0, 0], [2.0**-70, 0], [0, 1]], [], ExactBudget(max_bits=64))
        for options in ({"limit": True}, {"max_bits": 1}, {"max_edges": 0}):
            with self.assertRaises(ValueError):
                ExactBudget(**options)

    def test_frozen_native_conflict_has_a_real_owner_and_a_real_uncovered_remainder(
        self,
    ):
        path = (
            Path(__file__).with_name("fixtures") / "basel-water-source-membership.npz"
        )
        with np.load(path) as data:
            sources = shapely.get_parts(shapely.from_wkb(data["sources_wkb"].tobytes()))
            triangle = shapely.from_wkb(data["triangle_wkb"].tobytes())
            vertices = np.array(triangle.exterior.coords)[:3]
            original = sources.copy()
        candidates = sorted(shapely.STRtree(sources).query(triangle))
        budget = ExactBudget()
        areas, missing = exact_overlap(vertices, sources[candidates], budget)
        self.assertEqual(
            [int(i) for i, a in zip(candidates, areas, strict=True) if a > 0], [46]
        )
        self.assertAlmostEqual(float(sum(areas)), 7.830836439073031e-7, delta=1e-20)
        self.assertGreater(missing, 0)
        self.assertLess(float(missing), 5e-13)
        np.testing.assert_array_equal(shapely.to_wkb(sources), shapely.to_wkb(original))
        np.testing.assert_array_equal(vertices, np.array(triangle.exterior.coords)[:3])
        self.assertLess(budget.operations, 1000)
        complete, complete_missing = exact_overlap(vertices, sources, ExactBudget())
        self.assertEqual(missing, complete_missing)
        self.assertEqual(
            {int(i): a for i, a in zip(candidates, areas, strict=True) if a > 0},
            {i: a for i, a in enumerate(complete) if a > 0},
        )

    def test_native_sources_keep_exact_membership_under_dyadic_scale_and_reflection(
        self,
    ):
        path = (
            Path(__file__).with_name("fixtures") / "basel-water-source-membership.npz"
        )
        with np.load(path) as data:
            sources = shapely.get_parts(shapely.from_wkb(data["sources_wkb"].tobytes()))
            triangle = np.array(
                shapely.from_wkb(data["triangle_wkb"].tobytes()).exterior.coords
            )[:3]
        expected, missing = exact_overlap(triangle, sources, ExactBudget())
        for factor in (0.5, 2.0, -1.0):
            transformed = shapely.transform(
                sources, lambda xy, factor=factor: xy * factor
            )
            actual, uncovered = exact_overlap(
                triangle * factor, transformed, ExactBudget()
            )
            scale = Fraction(factor) ** 2
            self.assertEqual(actual, tuple(a * scale for a in expected))
            self.assertEqual(uncovered, missing * scale)
        reflected = shapely.transform(sources, lambda xy: xy[:, ::-1])
        actual, uncovered = exact_overlap(triangle[:, ::-1], reflected, ExactBudget())
        self.assertEqual(actual, expected)
        self.assertEqual(uncovered, missing)

    def test_many_overlapping_sources_match_independent_integer_cell_coverage(self):
        rng = np.random.default_rng(970)
        for _ in range(12):
            polygons, expected, covered = [], [], set()
            for _ in range(10):
                x0, x1 = sorted(rng.choice(9, 2, replace=False).tolist())
                y0, y1 = sorted(rng.choice(9, 2, replace=False).tolist())
                cells = {(x, y) for x in range(x0, x1) for y in range(y0, y1)}
                polygons.append(box(x0, y0, x1, y1))
                expected.append(len(cells))
                covered.update(cells)
            areas, missing = exact_overlap(
                [[-1, -1], [31, -1], [-1, 31]], polygons, ExactBudget()
            )
            self.assertEqual(areas, tuple(expected))
            self.assertEqual(missing, 512 - len(covered))

    def test_integer_rectangles_match_an_independent_analytic_integral(self):
        # For x+y<=n, x,y>=0, each rectangle uses inclusion/exclusion of the
        # positive-part quadratic antiderivative, independently of slab events.
        n = 12
        for a, b, c, d in (
            (0, 2, 0, 3),
            (3, 9, 2, 11),
            (-3, 5, -1, 13),
            (9, 15, 5, 18),
        ):
            lo_x, lo_y, hi_x, hi_y = max(a, 0), max(c, 0), min(b, n), min(d, n)
            positive = lambda v: Fraction(max(0, v) ** 2, 2)
            expected = (
                positive(n - lo_x - lo_y)
                - positive(n - hi_x - lo_y)
                - positive(n - lo_x - hi_y)
                + positive(n - hi_x - hi_y)
            )
            areas, missing = exact_overlap(
                [[0, 0], [n, 0], [0, n]], [box(a, c, b, d)], ExactBudget()
            )
            self.assertEqual(areas, (expected,))
            self.assertEqual(missing, Fraction(n * n, 2) - expected)


if __name__ == "__main__":
    unittest.main()
