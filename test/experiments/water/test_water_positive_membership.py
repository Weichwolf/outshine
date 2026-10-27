import hashlib
import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from compare_membership_cohort import run
from inspect_source_membership import DEFAULT
from shapely.geometry import MultiPolygon, Polygon, box
from water_exact_membership import ExactBudget, ExactBudgetExceeded, exact_overlap
from water_orientation_predicates import orientations
from water_positive_membership import positive_overlap, prepare_source


class PositiveMembership(unittest.TestCase):
    def check(self, tri, source, expected):
        prepared = prepare_source(source)
        before = prepared.edges.tobytes()
        for exact in (False, True):
            self.assertEqual(
                positive_overlap(tri, prepared, ExactBudget(), exact=exact), expected
            )
        areas, _missing = exact_overlap(tri, [source], ExactBudget())
        self.assertEqual(areas[0] > 0, expected)
        self.assertEqual(prepared.edges.tobytes(), before)
        self.assertFalse(prepared.edges.flags.writeable)

    def test_containment_identical_boundary_and_small_corner_overlap(self):
        tri = [[0, 0], [4, 0], [0, 4]]
        for source in (
            box(-10, -10, 10, 10),
            Polygon(tri),
            Polygon([[0, 0], [0.1, 0], [0, 0.1]]),
            box(3, -1, 5, 2),
        ):
            self.check(tri, source, True)

    def test_boundary_only_contacts_and_disjoint_sources_have_no_positive_area(self):
        tri = [[0, 0], [4, 0], [0, 4]]
        for source in (
            box(4, 0, 5, 1),
            box(-1, -1, 0, 0),
            box(-5, 0, 0, 5),
            box(10, 10, 11, 11),
        ):
            self.check(tri, source, False)

    def test_holes_concavity_and_disconnected_parts(self):
        source = box(-1, -1, 10, 10).difference(box(1, 1, 5, 5))
        self.check([[2, 2], [3, 2], [2, 3]], source, False)
        self.check([[0, 0], [8, 0], [0, 8]], source, True)
        islands = MultiPolygon([box(0, 0, 1, 1), box(4, 0, 5, 1)])
        self.check([[2, 0], [3, 0], [2, 1]], islands, False)
        self.check([[0, 0], [8, 0], [0, 8]], islands, True)
        self.check(
            [[0, 0], [8, 0], [0, 8]],
            Polygon([(0, 0), (3, 0), (3, 1), (1, 1), (1, 3), (0, 3)]),
            True,
        )

    def test_clockwise_rings_triangles_and_binary_coordinate_changes(self):
        triangle = np.array([[0.0, 0], [4, 0], [0, 4]])
        source = box(-1, -1, 2, 5)
        for scale, offset in (
            (1, [2**30 / 32, -(2**29) / 32]),
            (-2, [17, -51]),
            (0.125, [-19, 37]),
        ):
            transformed = shapely.transform(
                source, lambda xy, s=scale, o=offset: xy * s + o
            )
            self.check(
                (triangle * scale + offset)[::-1], shapely.reverse(transformed), True
            )

    def test_subnormal_triangle_and_ulp_slivers_do_not_become_empty(self):
        tiny = np.nextafter(0.0, 1.0)
        self.check([[0, 0], [tiny, 0], [0, tiny]], box(-1, -1, 1, 1), True)
        self.check([[0, 0], [2, 0], [0, 2]], box(0, 0, 2.0**-45, 1), True)

    def test_filtered_orientation_matches_rationals_in_cancellation_and_underflow(self):
        rng = np.random.default_rng(970)
        points = rng.integers(-128, 129, (300, 3, 2)).astype(float)
        points *= np.exp2(rng.integers(-500, 15, (300, 1, 1)))
        difficult = np.array(
            [
                [[0, 0], [1, 1], [1, np.nextafter(1.0, 2.0)]],
                [[0, 0], [2.0**-1074, 0], [0, 2.0**-1074]],
                [[1e7, 1e7], [1e7 + 1e-7, 1e7 + 1e-7], [1e7 + 2e-7, 1e7 + 2e-7]],
            ]
        )
        points = np.concatenate((points, difficult))
        a, b = ExactBudget(), ExactBudget()
        filtered = orientations(points[:, 0], points[:, 1], points[:, 2], a)
        exact = orientations(points[:, 0], points[:, 1], points[:, 2], b, exact=True)
        np.testing.assert_array_equal(filtered, exact)
        self.assertGreater(a.counts["exact_orientations"], 0)
        self.assertLess(a.counts["exact_orientations"], b.counts["exact_orientations"])

    def test_bad_inputs_and_budget_refusal_are_explicit(self):
        source = prepare_source(box(0, 0, 1, 1))
        for budget in (ExactBudget(limit=0), ExactBudget(limit=1)):
            with self.assertRaises(ExactBudgetExceeded):
                positive_overlap([[0, 0], [1, 0], [0, 1]], source, budget)
            self.assertLessEqual(budget.operations, budget.limit)
        for tri in (
            [[0, 0], [0, 0], [1, 1]],
            [[0, 0], [1e9, 0], [0, 1]],
            [[0, 0], [1, 0], [0, float("nan")]],
        ):
            with self.assertRaises(ValueError):
                positive_overlap(tri, source, ExactBudget())
        with self.assertRaises(ValueError):
            prepare_source(Polygon())
        with self.assertRaises(ValueError):
            prepare_source(Polygon([(0, 0), (1, 1), (0, 1), (1, 0)]))

    def test_cohort_budget_keeps_unresolved_faces_explicit_without_partial_ids(self):
        _s, _u, _t, report = run(DEFAULT, "filtered", global_work=1)
        self.assertFalse(report["all_complete"])
        self.assertEqual(report["total_counted_work"], 1)
        self.assertEqual(len(report["rows"]), report["faces"])
        self.assertTrue(all("members" not in r for r in report["rows"]))
        self.assertTrue(any(r["state"] == "global_budget" for r in report["rows"]))
        with self.assertRaises(ValueError):
            run(DEFAULT, "filtered", max_faces=1)
        with self.assertRaises(ValueError):
            run(DEFAULT, "nearest_owner")

    def test_native_partial_membership_conflict_matches_independent_exact_areas(self):
        folder = Path(__file__).with_name("fixtures")
        row = json.loads((folder / "basel-membership-partial.json").read_text())
        with np.load(folder / "basel-water-source-membership.npz") as data:
            sources = shapely.get_parts(shapely.from_wkb(data["sources_wkb"].tobytes()))
        self.assertEqual(
            hashlib.sha256(
                (folder / "basel-water-source-membership.npz").read_bytes()
            ).hexdigest(),
            row["original_fixture_sha256"],
        )
        tri = row["triangle_xy_m"]
        areas, _missing = exact_overlap(tri, sources, ExactBudget())
        expected = [i for i, area in enumerate(areas) if area > 0]
        self.assertEqual(expected, row["exact_positive_members"])
        for exact in (False, True):
            members = [
                i
                for i, source in enumerate(sources)
                if positive_overlap(
                    tri, prepare_source(source), ExactBudget(), exact=exact
                )
            ]
            self.assertEqual(members, expected)


if __name__ == "__main__":
    unittest.main()
