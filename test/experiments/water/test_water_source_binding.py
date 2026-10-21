import hashlib
import json
import unittest
from fractions import Fraction
from pathlib import Path

import numpy as np
import shapely
from shapely.geometry import Polygon, box
from water_exact_membership import ExactBudget, exact_overlap
from water_positive_membership import OverlapRelation, overlap_relation, prepare_source
from water_source_binding import NO_SOURCE, BindingBudget, bind_sources


class SourceBindingTests(unittest.TestCase):
    def layout(self):
        vertices = np.array([[0.0, 0], [1, 0], [0, 1], [10, 0], [11, 0], [10, 1]])
        faces = np.array([[0, 1, 2], [3, 4, 5]], dtype=np.uint32)
        sources = [box(0, 0, 2, 2), box(0, 0, 2, 2), box(10, 0, 12, 2)]
        return vertices, faces, sources, np.array([20, 3, 50], dtype=np.uint32)

    def test_native_shaped_csr_and_tied_representatives_need_no_areas(self):
        args = self.layout()
        result = bind_sources(*args)
        self.assertTrue(result.report["valid"])
        np.testing.assert_array_equal(result.offsets, [0, 2, 3])
        np.testing.assert_array_equal(result.sources, [3, 20, 50])
        np.testing.assert_array_equal(result.representatives, [3, 50])
        self.assertEqual(result.report["area_evaluations"], 0)
        self.assertEqual(result.report["paths"], {"covered": 1, "single": 1})
        oracle = bind_sources(*args, method="areas")
        for key in ("offsets", "sources", "representatives"):
            np.testing.assert_array_equal(getattr(result, key), getattr(oracle, key))
            self.assertFalse(getattr(result, key).flags.writeable)

    def test_multiple_partial_sources_use_exact_maximum_area(self):
        vertices = np.array([[0.0, 0], [4, 0], [0, 4]])
        faces = np.array([[0, 1, 2]], dtype=np.uint32)
        sources = [box(0, 0, 1, 4), box(0, 0, 4, 2)]
        result = bind_sources(vertices, faces, sources, np.array([5, 99]))
        np.testing.assert_array_equal(result.sources, [5, 99])
        np.testing.assert_array_equal(result.representatives, [99])
        self.assertEqual(result.report["area_evaluations"], 1)
        self.assertEqual(result.report["paths"], {"partial_areas": 1})
        tied = bind_sources(
            vertices, faces, [box(0, 0, 2, 4), box(0, 0, 4, 2)], np.array([9, 7])
        )
        np.testing.assert_array_equal(tied.representatives, [7])

    def test_original_id_order_permutation_and_immutable_inputs(self):
        vertices, faces, sources, ids = self.layout()
        before = vertices.copy(), faces.copy(), ids.copy()
        a = bind_sources(vertices, faces, sources, ids)
        order = [2, 0, 1]
        b = bind_sources(vertices, faces, [sources[i] for i in order], ids[order])
        for key in ("offsets", "sources", "representatives"):
            np.testing.assert_array_equal(getattr(a, key), getattr(b, key))
        for x, y in zip((vertices, faces, ids), before, strict=True):
            np.testing.assert_array_equal(x, y)

    def test_binding_validity_does_not_claim_full_geometric_coverage(self):
        triangle = np.array([[0.0, 0], [4, 0], [0, 4]])
        source = box(0, 0, 1, 4)
        result = bind_sources(triangle, np.array([[0, 1, 2]]), [source], np.array([6]))
        self.assertTrue(result.report["valid"])
        self.assertEqual(result.report["area_evaluations"], 0)
        areas, missing = exact_overlap(triangle, [source], ExactBudget())
        self.assertEqual(areas, (Fraction(7, 2),))
        self.assertGreater(missing, 0)

    def test_relation_matches_independent_exact_single_source_coverage(self):
        triangle = np.array([[0.0, 0], [4, 0], [0, 4]])
        for source in (
            box(-1, -1, 6, 6),
            Polygon(triangle),
            box(0, 0, 1, 4),
            box(9, 9, 10, 10),
            box(-1, -1, 6, 6).difference(box(0.5, 0.5, 1, 1)),
        ):
            areas, missing = exact_overlap(triangle, [source], ExactBudget())
            relation = overlap_relation(triangle, prepare_source(source), ExactBudget())
            self.assertEqual(relation == OverlapRelation.COVERS_TRIANGLE, missing == 0)
            self.assertEqual(relation != OverlapRelation.DISJOINT, areas[0] > 0)

    def test_member_refusal_is_atomic_and_independent_faces_still_bind(self):
        result = bind_sources(*self.layout(), budget=BindingBudget(memberships=1))
        self.assertFalse(result.report["valid"])
        np.testing.assert_array_equal(result.offsets, [0, 0, 1])
        np.testing.assert_array_equal(result.sources, [50])
        np.testing.assert_array_equal(result.representatives, [NO_SOURCE, 50])
        self.assertEqual(result.states, ("membership budget", "complete"))
        self.assertEqual(result.report["unused_source_ids"], [3, 20])

    def test_intersection_and_work_caps_skip_unnecessary_queries(self):
        for budget in (BindingBudget(intersections=0), BindingBudget(total_work=0)):
            result = bind_sources(*self.layout(), budget=budget)
            self.assertFalse(result.report["valid"])
            self.assertEqual(result.report["candidate_intersections"], 0)
            self.assertEqual(len(result.sources), 0)
            self.assertTrue(np.all(result.representatives == NO_SOURCE))
            self.assertLessEqual(result.report["counted_operations"], budget.total_work)

    def test_frozen_numeric_owner_error_selects_a_strictly_larger_original_source(self):
        folder = Path(__file__).with_name("fixtures")
        row = json.loads((folder / "basel-membership-owner.json").read_text())
        path = folder / "basel-water-source-membership.npz"
        self.assertEqual(
            hashlib.sha256(path.read_bytes()).hexdigest(), row["source_input_sha256"]
        )
        with np.load(path) as data:
            sources = shapely.get_parts(shapely.from_wkb(data["sources_wkb"].tobytes()))
            ids = data["source_indices"]
        vertices = np.array(row["triangle_xy_m"])
        result = bind_sources(vertices, np.array([[0, 1, 2]]), sources, ids)
        self.assertEqual(result.states, ("complete",))
        self.assertEqual(
            int(result.representatives[0]), row["exact_representative_native_id"]
        )
        pair = [
            row["exact_representative_native_id"],
            row["observed_numeric_representative_native_id"],
        ]
        positions = [int(np.flatnonzero(ids == source)[0]) for source in pair]
        areas, _missing = exact_overlap(vertices, sources[positions], ExactBudget())
        self.assertGreater(areas[0], areas[1])

    def test_missing_sources_and_invalid_inputs_are_never_hidden(self):
        vertices, faces, sources, ids = self.layout()
        result = bind_sources(vertices, faces, [box(100, 100, 101, 101)], np.array([7]))
        self.assertFalse(result.report["valid"])
        self.assertEqual(result.states, ("no_positive_source", "no_positive_source"))
        self.assertEqual(result.report["unused_source_ids"], [7])
        for bad_ids in (
            np.array([1.0, 2, 3]),
            np.array([3, 3, 4]),
            np.array([1, 2, NO_SOURCE]),
        ):
            with self.assertRaises(ValueError):
                bind_sources(vertices, faces, sources, bad_ids)
        with self.assertRaises(ValueError):
            BindingBudget(memberships=True)
        with self.assertRaises(ValueError):
            bind_sources(vertices, np.array([[0, 1, 99]]), sources, ids)


if __name__ == "__main__":
    unittest.main()
