"""Actual heightless native source snapshot → bound DEM → canonical port values."""

import math
import unittest
from dataclasses import replace

import numpy as np
from canonical_port_native_inputs import feature_spans, interior_demands, load
from outshine_ref.transport.canonical_ports import PortBudget, derive_canonical_ports


class NativeCanonicalPorts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source, terrain, cls.terrain_binding, cls.manifest = load()
        cls.terrain = staticmethod(terrain)
        cls.service = feature_spans(cls.source, 1047395490, 4)
        cls.path = feature_spans(cls.source, 1604876210, 11)
        cls.open_path = feature_spans(cls.source, 1604876210, 6)
        cls.demands = interior_demands(cls.source, cls.service + cls.path)

    def derive(self, demands, terrain=None, **kwargs):
        return derive_canonical_ports(
            self.source,
            tuple(demands),
            terrain if terrain is not None else self.terrain,
            terrain_binding=self.terrain_binding,
            maximum_prior_change_m=0.5,
            **kwargs,
        )

    def test_real_snapshot_ownership_and_complete_input_component_incidence(self):
        self.assertEqual(len(self.source.nodes), 4433)
        self.assertEqual(len(self.source.spans), 4743)
        self.assertEqual((len(self.service), len(self.path)), (5, 6))
        self.assertEqual(
            self.source.binding["sources"][0]["Identity"],
            {"DatasetId": "direct", "Revision": ""},
        )
        self.assertEqual(len(self.source.binding["omissions"]), 254)
        for component in (self.service, self.path):
            selected = {span.identifier for span in component}
            nodes = {node for span in component for node in span.nodes}
            touched = {
                span.identifier
                for span in self.source.spans
                if any(node in nodes for node in span.nodes)
            }
            self.assertEqual(touched, selected)
            self.assertFalse(nodes.intersection(self.source.unknown_nodes))
        self.assertTrue(
            any(
                span.identifier[1]
                != span.attributes["native_source_reference"]["SegmentOrdinal"]
                for span in self.path
            )
        )
        self.assertTrue(all(span.identifier[0] > 2**53 for span in self.path))

    def test_real_dem_ports_and_reverse_core_order_are_exact(self):
        forward = self.derive(self.demands)
        backward = self.derive(self.demands[::-1])
        self.assertEqual(forward.values, backward.values[::-1])
        self.assertEqual(forward.reasons, ("",) * 10)
        methods = {
            row["field"]["selected_method"] for row in forward.report["components"]
        }
        self.assertEqual(methods, {"affine", "cone"})
        for value in forward.values:
            self.assertGreater(value.height_m, 100)
            self.assertLess(value.height_m, 250)
            self.assertEqual(
                value.source_attributes["native_path"]["PathId"], value.edge[0]
            )
            direction, plane = np.array(value.direction_en), np.array(value.plane_en)
            self.assertAlmostEqual(np.linalg.norm(direction), 1)
            self.assertAlmostEqual(value.longitudinal_grade, plane @ direction)
            self.assertAlmostEqual(
                value.crossfall_rad, -math.atan(plane @ [-direction[1], direction[0]])
            )
        self.assertTrue(
            any(abs(value.longitudinal_grade) > 0.001 for value in forward.values)
        )
        self.assertTrue(
            any(abs(value.crossfall_rad) > 0.001 for value in forward.values)
        )

    def test_open_actual_native_component_remains_pending_before_dem_work(self):
        demands = interior_demands(self.source, self.open_path)
        self.assertTrue(demands)
        calls = []

        def forbidden(points):
            calls.append(len(points))
            raise AssertionError("open source component must not sample DEM")

        result = self.derive(demands[:1], forbidden)
        self.assertIsNone(result.values[0])
        self.assertEqual(result.reasons[0], "unknown_source_frontier")
        self.assertEqual(len(result.report["components"][0]["selected"]), 591)
        self.assertEqual(calls, [])

    def test_terrain_budget_is_shared_across_components_and_port_queries(self):
        full = self.derive(self.demands)
        limit = full.report["work"]["terrain_samples"] - 1
        calls = []

        def counted(points):
            calls.append(len(points))
            return self.terrain(points)

        limited = self.derive(
            self.demands, counted, budget=PortBudget(terrain_samples=limit)
        )
        self.assertEqual(sum(calls), limit)
        self.assertEqual(limited.report["work"]["terrain_samples"], limit)
        self.assertEqual(sum(value is None for value in limited.values), 1)
        self.assertIn(
            "terrain_sample_budget",
            next(reason for reason in limited.reasons if reason),
        )
        reversed_result = self.derive(
            self.demands[::-1], budget=PortBudget(terrain_samples=limit)
        )
        self.assertEqual(limited.values, reversed_result.values[::-1])
        self.assertEqual(limited.reasons, reversed_result.reasons[::-1])

    def test_real_budget_missing_dem_and_unsupported_height_layer_are_pending(self):
        demand = interior_demands(self.source, self.service)[0]
        limited = self.derive([demand], budget=PortBudget(selected_spans=4))
        self.assertIsNone(limited.values[0])
        self.assertIn("selected_span_budget", limited.reasons[0])

        def missing(_points):
            raise FileNotFoundError("bound original DEM unavailable")

        unavailable = self.derive([demand], missing)
        self.assertIsNone(unavailable.values[0])
        self.assertIn("bound original DEM unavailable", unavailable.reasons[0])
        changed = replace(self.service[0], layer=1)
        altered = replace(
            self.source,
            spans=tuple(
                changed if s.identifier == changed.identifier else s
                for s in self.source.spans
            ),
        )
        result = derive_canonical_ports(
            altered,
            (demand,),
            lambda _p: self.fail("layer height invented"),
            terrain_binding=self.terrain_binding,
            maximum_prior_change_m=0.5,
        )
        self.assertEqual(result.reasons, ("layer_height_recipe_required",))


if __name__ == "__main__":
    unittest.main()
