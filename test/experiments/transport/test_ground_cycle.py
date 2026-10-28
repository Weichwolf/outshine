"""One actual whole source loop; original authority and tree defaults stay strict."""

import unittest
from dataclasses import replace
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from ground_cycle import generate, prepare_cycle, source_cycle
from ground_cycle_checks import audit
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground_network_geometry import layout
from replay_ground_cycle import prepare


def index_for(edges):
    nodes = {
        i: (16.37 + (i % 2) * 0.0002, 48.21 + (i // 2) * 0.0002)
        for i in {n for edge in edges for n in edge}
    }
    incidence = {n: [] for n in nodes}
    spans = {}
    for i, pair in enumerate(edges):
        key = (i, 0)
        spans[key] = SimpleNamespace(
            nodes=pair,
            width_m=1.8,
            maximum_gradient=0.15,
            bridge=False,
            tunnel=False,
            layer=0,
        )
        for node in pair:
            incidence[node].append(key)
    return nodes, spans, incidence


class CycleTopology(unittest.TestCase):
    def test_whole_simple_ring_keeps_original_nodes_without_xy_inference(self):
        index = index_for(((0, 1), (1, 3), (3, 2), (2, 0)))
        anchor, leaves, ring = source_cycle(index, tuple(index[1]))
        self.assertEqual(anchor, 0)
        self.assertFalse(leaves)
        self.assertEqual(set(ring), {0, 1, 2, 3})

    def test_unsupported_cycles_and_incomplete_components_fail_before_dem(self):
        for pairs in (
            ((0, 1), (1, 2), (2, 3)),
            ((0, 1), (1, 2), (2, 0), (0, 3), (3, 2)),
            ((0, 1), (1, 2), (2, 0), (3, 4), (4, 5), (5, 3)),
            ((0, 1), (0, 1), (1, 2)),
            ((0, 0), (0, 1), (1, 2)),
        ):
            index = index_for(pairs)
            with self.assertRaises(ValueError):
                source_cycle(index, tuple(index[1]))
        index = index_for(((0, 1), (1, 3), (3, 2), (2, 0)))
        index[2][0].append((99, 0))
        with patch("ground_cycle.layout") as geometry:
            with self.assertRaisesRegex(ValueError, "incomplete_cycle_component"):
                prepare_cycle(
                    index, tuple(index[1]), self.fail, PortBudget(), deadline=1e30
                )
            geometry.assert_not_called()

    def test_bad_levels_profiles_and_reservation_budget_never_reach_geometry(self):
        for field, value in (
            ("layer", 1),
            ("bridge", True),
            ("maximum_gradient", None),
        ):
            index = index_for(((0, 1), (1, 3), (3, 2), (2, 0)))
            setattr(index[1][(0, 0)], field, value)
            with patch("ground_cycle.layout") as geometry:
                with self.assertRaises(ValueError):
                    prepare_cycle(
                        index, tuple(index[1]), self.fail, PortBudget(), deadline=1e30
                    )
                geometry.assert_not_called()

    def test_original_tree_default_still_rejects_a_hole(self):
        xy = np.array([[0, 0], [20, 0], [20, 20], [0, 20]], dtype=float)
        edges = np.array([[0, 1], [1, 2], [2, 3], [3, 0]])
        with self.assertRaisesRegex(ValueError, "unexpected_hole"):
            layout(xy, edges, np.full(4, 1.8), np.full(4, 0.15), seconds=5)
        raw, report = layout(
            xy, edges, np.full(4, 1.8), np.full(4, 0.15), seconds=5, expected_holes=1
        )
        self.assertIsNone(report["recipe"])
        domain = shapely.coverage_union_all(shapely.polygons(raw["xy"][raw["faces"]]))
        self.assertEqual(len(domain.interiors), 1)

    def test_short_ring_edge_and_unconnected_geometric_crossing_stay_rejected(self):
        edges = np.array([[0, 1], [1, 2], [2, 3], [3, 0]])
        for xy in (
            np.array([[0, 0], [0.05, 0], [20, 20], [0, 20]], dtype=float),
            np.array([[0, 0], [20, 20], [0, 20], [20, 0]], dtype=float),
        ):
            with self.assertRaises(ValueError):
                layout(
                    xy,
                    edges,
                    np.full(4, 1.8),
                    np.full(4, 0.15),
                    seconds=5,
                    expected_holes=1,
                )


class ActualCycle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.index, cls.selected, terrain, cls.budget, cls.metadata = prepare()
        cls.terrain = staticmethod(terrain)
        cls.result = generate(cls.index, cls.selected, cls.terrain, cls.budget)

    def test_six_source_spans_twelve_whole_sections_and_actual_hole_are_qualified(self):
        checked = audit(self.result)
        self.assertEqual(len(self.result["source_members"]), 6)
        self.assertEqual((checked["vertices"], checked["triangles"]), (32, 32))
        self.assertEqual(checked["whole_sections"], 12)
        self.assertEqual(checked["holes"], 1)
        self.assertGreater(checked["hole_area_m2"], 80)
        self.assertLessEqual(
            self.result["quality"]["maximum_original_prior_vertex_error_m"], 0.5
        )
        self.assertLessEqual(
            self.result["quality"]["finite_contacts"][
                "maximum_finite_contact_grade_jump"
            ],
            0.005,
        )

    def test_original_unknown_and_unregistered_shared_admission_are_not_erased(self):
        self.assertEqual(self.metadata["qualified_world_joins"], 0)
        self.assertEqual(self.metadata["qualified_original_end_contacts"], 0)
        self.assertEqual(self.metadata["closure_reason"], "unknown_source_frontier")
        self.assertEqual(
            self.metadata["strict_existing_tree_entry_reason"],
            "bounded_ground_tree_topology_required",
        )
        nodes = {n for e in self.selected for n in self.index[1][e].nodes}
        self.assertTrue(nodes <= set(self.metadata["original_unknown_nodes"]))

    def test_source_and_selected_order_cannot_change_frame_or_whole_geometry(self):
        reversed_index = tuple(
            dict(reversed(list(part.items()))) for part in self.index
        )
        result = generate(
            reversed_index, self.selected[::-1], self.terrain, self.budget
        )
        np.testing.assert_array_equal(
            result["mesh"].vertices, self.result["mesh"].vertices
        )
        np.testing.assert_array_equal(
            result["mesh"].triangles, self.result["mesh"].triangles
        )

    def test_linear_only_alternative_obeys_same_full_geometry_quality(self):
        other = generate(
            self.index, self.selected, self.terrain, self.budget, cut_method="linear"
        )
        self.assertEqual(audit(other), audit(self.result))
        self.assertEqual(
            sum(r["method"] == "linear" for r in self.result["geometry"]["cut_fits"]), 2
        )
        self.assertEqual(
            sum(r["method"] == "linear" for r in other["geometry"]["cut_fits"]), 4
        )

    def test_missing_dem_budget_and_filled_hole_cannot_be_released(self):
        with self.assertRaisesRegex(
            ValueError, "missing_or_invalid_bound_terrain_samples"
        ):
            generate(
                self.index,
                self.selected,
                lambda points: np.full(len(points), np.nan),
                self.budget,
            )
        budget = replace(
            self.budget, surface=replace(self.budget.surface, max_vertices=3)
        )
        with self.assertRaisesRegex(ValueError, "input_budget"):
            generate(self.index, self.selected, self.terrain, budget)
        mesh = self.result["mesh"]
        vertices = np.array([[0, 0, 163.5], [40, 0, 163.5], [0, 40, 163.5]])
        with self.assertRaisesRegex(ValueError, "cycle_hole_topology"):
            audit(self.result | {"mesh": type(mesh)(vertices, np.array([[0, 1, 2]]))})


if __name__ == "__main__":
    unittest.main()
