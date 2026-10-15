"""Independent domains, mode bindings and isolated kernel/budget failure cases."""

import json
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
import transport_component_surfaces as components
from transport_flat_plan import plan
from transport_network import transport_widths
from transport_travel import compile_travel


def graphs_of(axes):
    roads = [
        {
            "line": shapely.LineString(xy),
            "width": 2.0,
            "properties": {"class": "minor", **props},
            "source_id": f"controlled:{index}",
        }
        for index, (xy, props) in enumerate(axes)
    ]
    recipes = json.loads(
        Path(__file__).with_name("transport_network_recipes.json").read_text()
    )
    accepted, graphs, _ = plan(roads, recipes, transport_widths())
    compile_travel(accepted, graphs, recipes["travelRecipes"])
    for graph in graphs.values():
        graph["half_widths"] = np.ones(len(graph["lines"]))
    return list(graphs.values())


def coverage(products):
    return shapely.union_all(
        [
            shapely.union_all(shapely.polygons(p["vertices"][p["indices"]]))
            for p in products
        ]
    )


class ComponentChecks(unittest.TestCase):
    def test_global_fast_path_and_independent_parts_are_geometrically_equal(self):
        axes = [
            ([[-10, 0], [10, 0]], {}),
            ([[0, 0], [0, 10]], {}),
            ([[40, 0], [50, 0]], {}),
        ]
        graphs = graphs_of(axes)
        fast, report = components.assemble(graphs, corner_radius=0)
        split, local = components.assemble(graphs, corner_radius=0, prefer_global=False)
        self.assertTrue(report["fast_path"] and report["complete"])
        self.assertTrue(local["complete"])
        self.assertEqual(len(split), 2)
        self.assertLess(coverage(fast).symmetric_difference(coverage(split)).area, 1e-7)
        self.assertEqual(
            sum(r["edges"] for r in local["components"]),
            sum(len(g["lines"]) for g in graphs),
        )

    def test_physical_overlap_joins_disconnected_axis_graphs(self):
        graphs = graphs_of([([[0, 0], [10, 0]], {}), ([[0, 1.5], [10, 1.5]], {})])
        products, report = components.assemble(
            graphs, corner_radius=0, prefer_global=False
        )
        self.assertTrue(report["complete"], report)
        self.assertEqual(len(products), 1)
        self.assertEqual(report["components"][0]["source_indices"], [0, 1])

    def test_nested_disjoint_part_does_not_fill_the_outer_ring_hole(self):
        ring = [[-10, -10], [10, -10], [10, 10], [-10, 10], [-10, -10]]
        products, report = components.assemble(
            graphs_of([(ring, {}), ([[-2, 0], [2, 0]], {})]),
            corner_radius=0,
            prefer_global=False,
        )
        self.assertTrue(report["complete"], report)
        self.assertEqual(len(products), 2)
        shape = coverage(products)
        self.assertTrue(shape.covers(shapely.Point(0, 0)))
        self.assertFalse(shape.covers(shapely.Point(5, 5)))

    def test_local_edge_budget_keeps_the_unrelated_small_part(self):
        graphs = graphs_of(
            [
                ([[-10, 0], [10, 0]], {}),
                ([[0, 0], [0, 10]], {}),
                ([[40, 0], [50, 0]], {}),
            ]
        )
        products, report = components.assemble(
            graphs,
            corner_radius=0,
            prefer_global=False,
            budget=components.ComponentBudget(maximum_component_edges=1),
        )
        self.assertFalse(report["complete"])
        self.assertEqual(len(products), 1)
        shape = coverage(products)
        self.assertTrue(shape.covers(shapely.Point(45, 0)))
        self.assertFalse(shape.covers(shapely.Point(0, 0)))
        self.assertIn(
            "component_edge_budget", [r["reason"] for r in report["components"]]
        )

    def test_failed_kernel_is_local_after_global_failure(self):
        graphs = graphs_of([([[-20, 0], [20, 0]], {}), ([[40, 0], [42, 0]], {})])
        original = components.solve_component

        def fault(selected, domain, *args):
            if domain.area > 50:
                raise ValueError("injected_geometry_kernel_failure")
            return original(selected, domain, *args)

        with (
            patch.object(
                components,
                "global_product",
                side_effect=ValueError("injected_global_kernel_failure"),
            ),
            patch.object(components, "solve_component", side_effect=fault),
        ):
            products, report = components.assemble(graphs, corner_radius=0)
        self.assertFalse(report["complete"])
        self.assertEqual(report["global_failure"], "injected_global_kernel_failure")
        self.assertEqual(len(products), 1)
        self.assertTrue(coverage(products).covers(shapely.Point(41, 0)))
        self.assertFalse(coverage(products).covers(shapely.Point(0, 0)))

    def test_permissions_and_original_edge_indices_are_not_recompiled(self):
        graphs = graphs_of(
            [
                ([[0, 0], [10, 0]], {"access": "private", "oneway": "yes"}),
                ([[40, 0], [50, 0]], {"class": "cycleway"}),
            ]
        )
        _, report = components.assemble(graphs, corner_radius=0, prefer_global=False)
        ids = [edge for row in report["components"] for edge in row["original_edges"]]
        self.assertEqual(
            sorted(ids),
            [[i, j] for i, g in enumerate(graphs) for j in range(len(g["lines"]))],
        )
        for graph in graphs:
            for index in range(len(graph["lines"])):
                selected = components.select_graph(graph, np.array([index]))
                for key in ("travel_public", "travel_restricted"):
                    np.testing.assert_array_equal(selected[key], graph[key][[index]])
                np.testing.assert_array_equal(
                    selected["vertices"][selected["edges"]],
                    graph["vertices"][graph["edges"][[index]]],
                )

    def test_zero_time_and_input_caps_do_not_publish_uncertified_surfaces(self):
        graphs = graphs_of([([[0, 0], [10, 0]], {}), ([[40, 0], [50, 0]], {})])
        for budget, reason in (
            (components.ComponentBudget(seconds=0), "component_time_budget"),
            (
                components.ComponentBudget(maximum_edges=1),
                "component_input_edge_budget",
            ),
        ):
            products, report = components.assemble(graphs, budget=budget)
            self.assertEqual(products, [])
            self.assertEqual(report["preparation_reason"], reason)

    def test_total_output_limit_retains_one_complete_small_body(self):
        graphs = graphs_of([([[0, 0], [2, 0]], {}), ([[40, 0], [50, 0]], {})])
        products, report = components.assemble(
            graphs,
            corner_radius=0,
            budget=components.ComponentBudget(maximum_faces=8),
        )
        self.assertFalse(report["complete"])
        self.assertEqual(report["global_failure"], "global_output_face_budget")
        self.assertEqual(len(products), 1)
        self.assertLessEqual(sum(len(p["indices"]) for p in products), 8)
        self.assertTrue(coverage(products).covers(shapely.Point(1, 0)))
        self.assertFalse(coverage(products).covers(shapely.Point(45, 0)))
        self.assertIn(
            "component_total_face_budget", [r["reason"] for r in report["components"]]
        )


if __name__ == "__main__":
    unittest.main()
