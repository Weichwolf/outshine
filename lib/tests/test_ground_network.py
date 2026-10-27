"""Whole-width conformity and bounded source-tree construction controls."""

import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground_network_geometry import layout
from outshine_ref.transport.canonical_ports._ground_network_input import (
    prepare_network_ground,
)
from outshine_ref.transport.canonical_ports._ground_recipe import source_network


class GroundNetwork(unittest.TestCase):
    def setUp(self):
        self.xy = np.array(
            [[0, 0], [-12, 0], [10, 9], [15, 17], [11, -15], [14, -22.0]]
        )
        self.edges = np.array([[0, 1], [0, 2], [2, 3], [0, 4], [4, 5]])
        self.widths = np.full(5, 2.0)
        self.grades = np.full(5, 0.12)

    def build(self):
        return layout(self.xy, self.edges, self.widths, self.grades, seconds=5)

    def test_whole_original_bands_are_covered_without_overlapping_parts(self):
        raw, report = self.build()
        triangles = shapely.polygons(raw["xy"][raw["faces"]])
        self.assertTrue(shapely.coverage_is_valid(triangles))
        whole = shapely.coverage_union_all(triangles)
        bands = shapely.buffer(
            shapely.linestrings(self.xy[self.edges]), self.widths / 2, cap_style="flat"
        )
        required = shapely.union_all(bands)
        self.assertLessEqual(shapely.difference(required, whole).area, 1e-8)
        self.assertEqual(whole.geom_type, "Polygon")
        self.assertFalse(whole.interiors)
        self.assertEqual(len(raw["caps"]), 3)
        self.assertEqual(len(report["cut_fits"]), 3)

    def test_each_port_is_one_whole_shared_edge_with_correct_part_roles(self):
        raw, _ = self.build()
        adjacent = {}
        for corners, part in zip(raw["faces"], raw["owners"], strict=True):
            for a, b in ((0, 1), (1, 2), (2, 0)):
                edge = tuple(sorted(map(int, corners[[a, b]])))
                adjacent.setdefault(edge, []).append(part)
        self.assertLessEqual(max(map(len, adjacent.values())), 2)
        for edge, parts in zip(raw["ports"], raw["links"], strict=True):
            self.assertEqual(set(adjacent[tuple(sorted(map(int, edge)))]), set(parts))
            self.assertEqual(int(raw["roles"][parts].sum()), 1)
        boundary = np.array([e for e, parts in adjacent.items() if len(parts) == 1])
        self.assertTrue((np.unique(boundary, return_counts=True)[1] == 2).all())

    def test_crossing_unconnected_corridors_are_rejected(self):
        # The last arm crosses another retained arm without a source node.
        self.xy = np.array(
            [[0, 0], [30, 0], [30, 30], [-40, 30], [-30, 0], [-30, 60], [0, -20.0]]
        )
        self.edges = np.array([[0, 1], [1, 2], [2, 3], [0, 4], [4, 5], [0, 6]])
        self.widths, self.grades = np.full(6, 2.0), np.full(6, 0.12)
        with self.assertRaisesRegex(ValueError, "overlap|nonconforming"):
            self.build()

    def test_cuts_do_not_consume_a_short_source_edge(self):
        self.xy[1] = [-0.11, 0]
        with self.assertRaises(ValueError):
            self.build()

    def test_terminal_plane_keeps_road_grade_separate_from_core_cross_grade(self):
        raw, report = self.build()
        cap = raw["caps"][0]
        faces = np.flatnonzero(np.isin(raw["faces"], cap).sum(axis=1) == 2)
        self.assertEqual(len(faces), 1)
        self.assertEqual(raw["roles"][raw["owners"][faces[0]]], 1)
        a, b = self.edges[report["terminal_edge_indices"][0]]
        tangent = self.xy[b] - self.xy[a]
        tangent /= np.linalg.norm(tangent)
        gradient = 0.1 * tangent
        self.assertLessEqual(np.linalg.norm(raw["frames"][faces[0]] @ gradient), 1)

    def test_four_leaf_queries_are_budgeted_before_geometry_or_dem(self):
        spans = {
            (i, 0): SimpleNamespace(
                nodes=(0, i),
                width_m=2.0,
                maximum_gradient=0.12,
                bridge=False,
                tunnel=False,
                layer=0,
            )
            for i in range(1, 5)
        }

        def forbidden(_):
            self.fail("underbudgeted four-way request reached DEM")

        with self.assertRaisesRegex(ValueError, "ground_terminal_query_budget"):
            prepare_network_ground(
                ({}, spans, {}),
                tuple(spans),
                forbidden,
                PortBudget(requests=3),
                deadline=1e100,
            )

    def test_source_guard_accepts_a_tree_and_rejects_cycles_and_other_levels(self):
        spans = {}
        for i, nodes in enumerate(self.edges):
            spans[(i + 1, 0)] = SimpleNamespace(
                nodes=tuple(map(int, nodes)),
                width_m=2.0,
                maximum_gradient=0.12,
                bridge=False,
                tunnel=False,
                layer=0,
            )
        index = ({}, spans, {})
        shape, reason = source_network(index, tuple(spans))
        self.assertFalse(reason)
        self.assertEqual(shape, (0, (1, 3, 5)))
        spans[(6, 0)] = SimpleNamespace(**(vars(spans[(1, 0)]) | {"nodes": (1, 3)}))
        self.assertEqual(
            source_network(index, tuple(spans))[1],
            "bounded_ground_tree_topology_required",
        )
        del spans[(6, 0)]
        spans[(1, 0)].bridge = True
        self.assertEqual(
            source_network(index, tuple(spans))[1], "ground_tree_profile_required"
        )


if __name__ == "__main__":
    unittest.main()
