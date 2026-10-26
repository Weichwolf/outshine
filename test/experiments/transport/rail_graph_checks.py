"""Independent graph ownership, switch/crossing gaps and whole-footprint contacts."""

import unittest

import numpy as np
import shapely
from outshine_ref.transport.rail_hardware import build_rail_hardware as build_track
from rail_graph_inputs import chain_recipe, rail_chains, safe_pieces


def graph(vertices, edges):
    vertices, edges = np.array(vertices, float), np.array(edges, int)
    return {
        "vertices": vertices,
        "edges": edges,
        "lines": np.array([shapely.LineString(vertices[e]) for e in edges], object),
        "eligible": np.ones(len(edges), bool),
        "sources": [(f"source-{i}",) for i in range(len(edges))],
    }


class RailGraph(unittest.TestCase):
    def test_degree_two_chain_keeps_reversed_edges_and_all_source_ids(self):
        g = graph([[0, 0], [10, 0], [20, 0]], [[1, 0], [1, 2]])
        chains, branches = rail_chains(g)
        self.assertEqual(len(chains), 1)
        self.assertEqual(len(branches), 0)
        np.testing.assert_array_equal(chains[0]["xy"], [[0, 0], [10, 0], [20, 0]])
        self.assertEqual(chains[0]["sources"], ["source-0", "source-1"])

    def test_branch_stops_chains_and_never_invents_a_switch(self):
        g = graph([[0, 0], [30, 0], [60, 10], [60, -10]], [[0, 1], [1, 2], [1, 3]])
        chains, branches = rail_chains(g)
        self.assertEqual(len(chains), 3)
        np.testing.assert_array_equal(branches, [[30, 0]])
        pieces, ledger, _ = safe_pieces(chains, branches, [], 1.31)
        self.assertGreater(len(pieces), 0)
        for piece in pieces:
            self.assertGreaterEqual(
                shapely.LineString(piece["xy"]).distance(shapely.Point(30, 0)), 5.9
            )
        self.assertTrue(any(r["excluded_length_m"] > 0 for r in ledger))

    def test_crossing_keeps_both_valid_outer_parts_and_whole_hardware_clear(self):
        chains = [{"xy": np.array([[-30.0, 0], [30, 0]]), "sources": ["rail"]}]
        land = {
            "lines": np.array([shapely.LineString([[0, -10], [0, 10]])], object),
            "half_widths": np.array([2.0]),
        }
        pieces, ledger, _ = safe_pieces(chains, np.empty((0, 2)), [land], 1.31)
        self.assertEqual(len(pieces), 2)
        bed = shapely.box(-31, -2, 31, 2)
        crossing = shapely.box(-2, -10, 2, 10)
        for piece in pieces:
            product, report = build_track(piece["xy"], 0.0)
            self.assertTrue(report["valid"])
            self.assertTrue(shapely.covers(bed, product.footprints).all())
            self.assertEqual(
                float(
                    shapely.area(
                        shapely.intersection(product.footprints, crossing)
                    ).sum()
                ),
                0,
            )
        self.assertAlmostEqual(
            ledger[0]["candidate_length_m"] + ledger[0]["excluded_length_m"], 60.0
        )

    def test_fully_excluded_span_is_empty_with_explicit_reason(self):
        chains = [{"xy": np.array([[0.0, 0], [20, 0]]), "sources": ["rail"]}]
        land = {
            "lines": np.array([shapely.LineString([[0, 0], [20, 0]])], object),
            "half_widths": np.array([2.0]),
        }
        pieces, ledger, _ = safe_pieces(chains, np.empty((0, 2)), [land], 1.31)
        self.assertFalse(pieces)
        self.assertEqual(ledger[0]["excluded_length_m"], 20.0)
        self.assertTrue(ledger[0]["exclusion_reason"])

    def test_crossed_rail_chains_have_no_overlapping_retained_hardware(self):
        chains = [
            {"xy": np.array([[-20.0, 0], [20, 0]]), "sources": ["a"]},
            {"xy": np.array([[0.0, -20], [0, 20]]), "sources": ["b"]},
        ]
        pieces, _, _ = safe_pieces(chains, np.empty((0, 2)), [], 1.31)
        products = [build_track(p["xy"], 0.0)[0] for p in pieces]
        for index, first in enumerate(products):
            a = shapely.union_all(first.footprints)
            for second in products[index + 1 :]:
                b = shapely.union_all(second.footprints)
                self.assertLessEqual(a.intersection(b).area, 1e-9)

    def test_gauge_tags_are_retained_and_ambiguous_gauge_is_not_guessed(self):
        self.assertEqual(
            chain_recipe(["a"], {"a": {"properties": {}}}),
            (1.435, "procedural_default"),
        )
        self.assertEqual(
            chain_recipe(["a"], {"a": {"properties": {"gauge": "1067"}}}),
            (1.067, "tag_bound"),
        )
        gauge, reason = chain_recipe(
            ["a"], {"a": {"properties": {"gauge": "1435;1067"}}}
        )
        self.assertIsNone(gauge)
        self.assertTrue(reason)


if __name__ == "__main__":
    unittest.main()
