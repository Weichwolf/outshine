"""Source semantics, complete-pair graph controls and locally bounded absence."""

import unittest
from unittest.mock import patch

import shapely
from water_body_domains import assemble
from water_domain_oracle import all_pairs, check_complete

SOURCE = {"origin": 0, "kind": 1, "provider": "provider", "revision": "r"}
INPUT = {
    "layer": "water_polygons",
    "kind": "river",
    "provider_feature_id": "1",
    "bridge": False,
    "tunnel": False,
    "crossing": "",
    "intermittent": False,
}


class WaterDomains(unittest.TestCase):
    def run_case(self, polygons, inputs=None, sources=None, **kwargs):
        inputs = inputs or [dict(INPUT) for _ in polygons]
        sources = sources or [dict(SOURCE) for _ in polygons]
        return assemble(polygons, inputs, sources, **kwargs)

    def test_area_line_and_point_contacts_match_complete_pairs(self):
        polygons = [
            shapely.box(0, 0, 2, 2),
            shapely.box(1, 1, 3, 3),
            shapely.box(3, 1, 4, 3),
            shapely.box(4, 3, 5, 4),
            shapely.box(10, 10, 11, 11),
        ]
        inputs = [dict(INPUT, provider_feature_id=str(i)) for i in range(5)]
        bodies, rejected, work = self.run_case(polygons, inputs, assume_ground=True)
        self.assertEqual(rejected, ())
        self.assertEqual(tuple(b.sources for b in bodies), ((0, 1, 2), (3,), (4,)))
        expected, _ = all_pairs(polygons, inputs, [SOURCE] * 5, assume_ground=True)
        check_complete(polygons, bodies, expected)
        self.assertEqual(work["point_only_contacts"], 1)

    def test_holes_and_overlapping_envelopes_are_not_positive_connections(self):
        shell = shapely.Polygon(
            [(0, 0), (8, 0), (8, 8), (0, 8)], holes=[[(2, 2), (6, 2), (6, 6), (2, 6)]]
        )
        polygons = [shell, shapely.box(3, 3, 4, 4)]
        bodies, rejected, work = self.run_case(polygons, assume_ground=True)
        self.assertEqual(rejected, ())
        self.assertEqual(tuple(b.sources for b in bodies), ((0,), (1,)))
        self.assertEqual(work["relation_tests"], 1)
        self.assertEqual(len(bodies[0].geometry.interiors), 1)

    def test_ground_assumption_is_explicit_and_can_do_zero_tree_work(self):
        polygons = [shapely.box(0, 0, 2, 2)] * 2
        with patch("water_body_domains.shapely.STRtree", side_effect=AssertionError):
            bodies, rejected, work = self.run_case(polygons)
        self.assertEqual(rejected, ())
        self.assertEqual(tuple(b.sources for b in bodies), ((0,), (1,)))
        self.assertEqual(work["tree_queries"], 0)
        self.assertFalse(work["assume_ground"])
        self.assertTrue(all(b.policy == "ground_unresolved" for b in bodies))

    def test_crossing_unknown_layer_revision_and_class_are_separate(self):
        polygons = [shapely.box(0, 0, 2, 2)] * 8
        inputs = [
            dict(INPUT),
            dict(INPUT, bridge=True, crossing="bridge"),
            dict(INPUT, tunnel=True, crossing="tunnel"),
            dict(INPUT, kind="unknown"),
            dict(INPUT, layer="water_lines"),
            None,
            dict(INPUT),
            dict(INPUT, bridge=None),
        ]
        sources = [dict(SOURCE) for _ in polygons]
        sources[6]["revision"] = "other"
        bodies, rejected, _ = self.run_case(
            polygons, inputs, sources, assume_ground=True
        )
        self.assertEqual(rejected, ())
        self.assertEqual(
            tuple(b.sources for b in bodies), tuple((i,) for i in range(8))
        )

    def test_still_water_only_merges_same_feature_in_same_provenance(self):
        polygons = [shapely.box(0, 0, 2, 2)] * 4
        inputs = [
            dict(INPUT, kind="lake", provider_feature_id=x)
            for x in ("1", "1", "2", None)
        ]
        bodies, rejected, _ = self.run_case(polygons, inputs, assume_ground=True)
        self.assertEqual(rejected, ())
        self.assertEqual(tuple(b.sources for b in bodies), ((0, 1), (2,), (3,)))

    def test_expensive_component_budget_preserves_independent_domains(self):
        polygons = [shapely.box(0, 0, 2, 2)] * 3 + [
            shapely.box(10, 0, 12, 2),
            shapely.box(12, 0, 14, 2),
            shapely.box(30, 0, 31, 1),
        ]
        bodies, rejected, work = self.run_case(
            polygons, assume_ground=True, relation_limit=1
        )
        self.assertEqual(tuple(b.sources for b in bodies), ((3, 4), (5,)))
        self.assertEqual([r["source"] for r in rejected], [0, 1, 2])
        self.assertEqual(work["relation_tests"], 1)
        self.assertTrue(all(r["reason"] == "relation_budget" for r in rejected))

    def test_candidate_bucket_budget_keeps_other_buckets_and_original_reasons(self):
        polygons = [shapely.box(0, 0, 2, 2)] * 4
        inputs = [dict(INPUT)] * 3 + [dict(INPUT, kind="pond", provider_feature_id="2")]
        bodies, rejected, work = self.run_case(
            polygons, inputs, assume_ground=True, candidate_limit=1
        )
        self.assertEqual(tuple(b.sources for b in bodies), ((3,),))
        self.assertEqual([r["source"] for r in rejected], [0, 1, 2])
        self.assertEqual(work["relation_tests"], 0)
        self.assertEqual(work["max_retained_pairs"], 1)

    def test_bad_geometry_and_hard_caps_do_not_erase_valid_sources(self):
        polygons = [
            shapely.Polygon([(0, 0), (2, 2), (2, 0), (0, 2)]),
            shapely.box(10, 0, 11, 1),
        ]
        bodies, rejected, _ = self.run_case(polygons, assume_ground=True)
        self.assertEqual(tuple(b.sources for b in bodies), ((1,),))
        self.assertEqual(rejected[0]["source"], 0)
        for kwargs in (
            {"source_limit": 1025},
            {"candidate_limit": 32769},
            {"relation_limit": 65537},
            {"coordinate_limit": 2},
        ):
            with self.assertRaises(ValueError):
                self.run_case(polygons, **kwargs)

    def test_seeded_domains_match_complete_pair_bfs(self):
        import numpy as np

        random = np.random.default_rng(880)
        for _ in range(12):
            xy = random.integers(0, 20, (30, 2))
            polygons = [shapely.box(x, y, x + 3, y + 3) for x, y in xy]
            inputs = [
                dict(
                    INPUT,
                    kind="river" if i % 2 else "pond",
                    provider_feature_id=str(i % 3),
                )
                for i in range(30)
            ]
            bodies, rejected, _ = self.run_case(polygons, inputs, assume_ground=True)
            self.assertEqual(rejected, ())
            expected, _ = all_pairs(polygons, inputs, [SOURCE] * 30, assume_ground=True)
            check_complete(polygons, bodies, expected)
            pair_bodies, pair_rejected, _ = self.run_case(
                polygons, inputs, assume_ground=True, broad_phase="tree"
            )
            self.assertEqual(pair_rejected, ())
            check_complete(polygons, pair_bodies, expected)

    def test_geometry_failure_is_local_to_the_uncertain_component(self):
        polygons = [shapely.box(0, 0, 2, 2)] * 2 + [shapely.box(10, 0, 12, 2)] * 2
        original = type(polygons[0]).intersection

        def fail_first(a, b, **kwargs):
            if a.bounds[0] < 5:
                raise shapely.errors.GEOSException("injected")
            return original(a, b, **kwargs)

        with patch.object(type(polygons[0]), "intersection", fail_first):
            bodies, rejected, work = self.run_case(polygons, assume_ground=True)
        self.assertEqual(tuple(b.sources for b in bodies), ((2, 3),))
        self.assertEqual([r["source"] for r in rejected], [0, 1])
        self.assertTrue(all(r["reason"] == "relation_geometry_error" for r in rejected))
        self.assertEqual(work["relation_tests"], 2)

    def test_empty_table_is_an_empty_complete_partition(self):
        bodies, rejected, work = assemble([], [], [])
        self.assertEqual((bodies, rejected), ((), ()))
        self.assertEqual(work["source_count"], 0)


if __name__ == "__main__":
    unittest.main()
