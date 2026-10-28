"""Whole original two-loop surfaces and strict source/default counterexamples."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from bridge_source_fact_inputs import load_bridge_source_facts
from ground_cycle import source_cycle
from ground_cyclic_checks import audit
from ground_cyclic_inputs import prepare
from ground_two_cycles import generate, prepare_two_cycles, source_two_cycles
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground_domain import mesh_parts
from outshine_ref.transport.canonical_ports._ground_network_geometry import layout
from replay_branched_ground import INPUT
from test_ground_cycle import index_for

PAIRS = ((0, 1), (1, 2), (2, 0), (0, 3), (3, 4), (4, 0))


class TwoCycleTopology(unittest.TestCase):
    def test_shared_original_node_forms_two_cycles_without_xy_inference(self):
        index = index_for(PAIRS)
        anchor, leaves = source_two_cycles(index, tuple(index[1]))
        self.assertEqual(anchor, 0)
        self.assertEqual(leaves, ())
        with self.assertRaisesRegex(ValueError, "bounded_single_cycle"):
            source_cycle(index, tuple(index[1]))

    def test_invalid_sources_fail_before_layout_or_dem(self):
        cases = (
            PAIRS[:-1],
            PAIRS + ((0, 0),),
            PAIRS + ((0, 1),),
            ((0, 1), (1, 2), (2, 0), (3, 4), (4, 5), (5, 3), (6, 7)),
        )
        for pairs in cases:
            index = index_for(pairs)
            with patch("ground_two_cycles.plan_source_surface") as construct:
                with self.assertRaises(ValueError):
                    prepare_two_cycles(
                        index, tuple(index[1]), self.fail, PortBudget(), deadline=1e30
                    )
                construct.assert_not_called()
        for field, value in (
            ("layer", 1),
            ("tunnel", True),
            ("maximum_gradient", None),
            ("width_m", np.nan),
        ):
            index = index_for(PAIRS)
            setattr(index[1][(0, 0)], field, value)
            with patch("ground_two_cycles.plan_source_surface") as construct:
                with self.assertRaises(ValueError):
                    prepare_two_cycles(
                        index, tuple(index[1]), self.fail, PortBudget(), deadline=1e30
                    )
                construct.assert_not_called()

    def test_extra_incidence_and_selected_span_budget_fail_before_construction(self):
        index = index_for(PAIRS)
        with patch("ground_two_cycles.plan_source_surface") as construct:
            with self.assertRaisesRegex(ValueError, "selected_span_budget"):
                prepare_two_cycles(
                    index,
                    tuple(index[1]),
                    self.fail,
                    replace(PortBudget(), selected_spans=5),
                    deadline=1e30,
                )
            index[2][0].append((99, 0))
            with self.assertRaisesRegex(ValueError, "incomplete"):
                prepare_two_cycles(
                    index, tuple(index[1]), self.fail, PortBudget(), deadline=1e30
                )
            construct.assert_not_called()

    def test_two_holes_require_explicit_new_contract_and_do_not_widen_defaults(self):
        xy = np.array(
            [[0, 0], [20, 0], [20, 20], [0, 20], [-20, 0], [-20, -20], [0, -20]], float
        )
        edges = np.array(
            [[0, 1], [1, 2], [2, 3], [3, 0], [0, 4], [4, 5], [5, 6], [6, 0]]
        )
        for holes in (0, 1):
            with self.assertRaisesRegex(ValueError, "unexpected_hole"):
                layout(
                    xy,
                    edges,
                    np.full(8, 1.8),
                    np.full(8, 0.15),
                    seconds=5,
                    expected_holes=holes,
                )
        raw, report = layout(
            xy, edges, np.full(8, 1.8), np.full(8, 0.15), seconds=5, expected_holes=2
        )
        domain = shapely.coverage_union_all(shapely.polygons(raw["xy"][raw["faces"]]))
        self.assertEqual(len(domain.interiors), 2)
        self.assertIsNone(report["recipe"])
        for invalid in (True, -1, 3):
            with self.assertRaisesRegex(ValueError, "hole_budget"):
                mesh_parts([], [], [], [], [], [], expected_holes=invalid)


class ActualTwoCycles(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.index, cls.cases, terrain, cls.budget, cls.metadata = prepare()
        cls.terrain = staticmethod(terrain)
        cls.results, cls.refusals = {}, {}
        for case in cls.cases:
            try:
                cls.results[case["key"]] = generate(
                    cls.index, case["selected"], cls.terrain, cls.budget
                )
            except ValueError as error:
                cls.refusals[case["key"]] = str(error)

    def test_all_five_original_components_have_whole_results_or_explicit_reasons(self):
        self.assertEqual(
            (len(self.cases), len(self.results), len(self.refusals)), (5, 2, 3)
        )
        self.assertEqual(
            set(self.refusals.values()),
            {"missing_source_profile_recipe", "two_cycle_parallel_or_self_span"},
        )
        self.assertEqual(
            sorted(
                (len(v["mesh"].vertices), len(v["mesh"].triangles))
                for v in self.results.values()
            ),
            [(72, 74), (81, 83)],
        )
        for result in self.results.values():
            checked = audit(result)
            self.assertEqual(checked["holes"], 2)
            self.assertEqual(checked["mesh_euler_characteristic"], -1)
            self.assertGreaterEqual(checked["whole_sections"], 24)
            self.assertLessEqual(
                result["quality"]["maximum_original_prior_vertex_error_m"], 0.5
            )
            self.assertLessEqual(
                result["quality"]["finite_contacts"][
                    "maximum_finite_contact_grade_jump"
                ],
                0.005,
            )

    def test_original_unknown_and_source_world_limits_remain_explicit(self):
        self.assertEqual(self.metadata["qualified_world_joins"], 0)
        self.assertEqual(self.metadata["qualified_original_end_contacts"], 0)
        unknown = set(load_bridge_source_facts(INPUT)[0].unknown_nodes)
        for case in self.cases:
            self.assertEqual(case["closure_reason"], "unknown_source_frontier")
            self.assertEqual(
                tuple(sorted(set(case["source_node_ids"]) & unknown)),
                case["original_unknown_nodes"],
            )

    def test_order_and_linear_alternative_preserve_complete_source_products(self):
        reversed_index = tuple(
            dict(reversed(list(part.items()))) for part in self.index
        )
        for case in self.cases:
            if case["key"] not in self.results:
                continue
            original = self.results[case["key"]]
            reversed_result = generate(
                reversed_index, case["selected"][::-1], self.terrain, self.budget
            )
            np.testing.assert_array_equal(
                original["mesh"].vertices, reversed_result["mesh"].vertices
            )
            np.testing.assert_array_equal(
                original["mesh"].triangles, reversed_result["mesh"].triangles
            )
            linear = generate(
                self.index,
                case["selected"],
                self.terrain,
                self.budget,
                cut_method="linear",
            )
            checked = audit(linear)
            self.assertEqual(checked["holes"], 2)
            for field in ("source_nodes_EN", "source_edges_local"):
                np.testing.assert_array_equal(
                    original["geometry"][field], linear["geometry"][field]
                )
            self.assertEqual(original["source_members"], linear["source_members"])
            self.assertLessEqual(
                linear["quality"]["maximum_original_prior_vertex_error_m"], 0.5
            )
            self.assertLessEqual(
                linear["quality"]["finite_contacts"][
                    "maximum_finite_contact_grade_jump"
                ],
                0.005,
            )

    def test_missing_dem_filled_holes_and_forged_original_plane_faces_fail(self):
        case = next(c for c in self.cases if c["key"] in self.results)
        with self.assertRaisesRegex(ValueError, "missing_or_invalid_bound_terrain"):
            generate(
                self.index,
                case["selected"],
                lambda points: np.full(len(points), np.nan),
                self.budget,
            )
        result = self.results[case["key"]]
        mesh = result["mesh"]
        vertices = np.array([[0, 0, 0], [100, 0, 0], [0, 100, 0]], float)
        with self.assertRaisesRegex(ValueError, "hole_topology"):
            audit(result | {"mesh": type(mesh)(vertices, np.array([[0, 1, 2]]))})
        forged = result["geometry"] | {
            "source_nodes_EN": np.asarray(result["geometry"]["source_nodes_EN"])
            + [1000, 0]
        }
        with self.assertRaisesRegex(ValueError, "original_hole_not_preserved"):
            audit(result | {"geometry": forged})


if __name__ == "__main__":
    unittest.main()
