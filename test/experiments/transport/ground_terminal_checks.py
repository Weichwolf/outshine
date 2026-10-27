"""Actual finite terminal sections, unchanged quality and independent port audits."""

import json
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import Mock

import numpy as np
import shapely
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from outshine_ref.contracts import Mesh
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._geodesy import frame, positions_in_frame
from outshine_ref.transport.canonical_ports._ground_scope_field import (
    derive_scoped_ground_field,
)
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)
from outshine_ref.transport.canonical_ports._ground_terminal_geometry import (
    rebuild_terminal_caps,
)
from outshine_ref.transport.canonical_ports._ground_terminals import (
    derive_scoped_ground_terminals,
)
from outshine_ref.transport.canonical_ports._queries import CoarseDomain, canonical_port

SEED = (8409897393650800292, 0)
OTHER = (11909413124558356807, 0)


class ActualGroundTerminals(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fixture = Path(__file__).parent / "fixtures/bridge-source-facts"
        cls.source, cls.groups, cls.ways, cls.binding, _ = load_bridge_source_facts(
            fixture
        )
        reference = json.loads((fixture / "manifest.json").read_text())[
            "terrain_reference"
        ]
        terrain, cls.terrain_binding = terrain_input(FIXTURE, {"terrain": reference})
        cls.terrain = staticmethod(terrain)
        cls.index, reason, _ = prepare_ground_scope_index(
            cls.source,
            cls.groups,
            cls.ways,
            source_binding=cls.source.binding,
            candidate_binding=cls.binding,
        )
        assert not reason, reason

    def derive(
        self,
        *,
        method="auto",
        seed=SEED,
        limit=3,
        budget=None,
        source=None,
        groups=None,
        terrain=None,
        scope_index=None,
    ):
        return derive_scoped_ground_terminals(
            self.source if source is None else source,
            seed,
            self.groups if groups is None else groups,
            self.ways,
            self.terrain if terrain is None else terrain,
            source_binding=self.source.binding,
            candidate_binding=self.binding,
            terrain_binding=self.terrain_binding,
            maximum_prior_change_m=limit,
            method=method,
            budget=budget,
            scope_index=self.index if scope_index is None else scope_index,
        )

    def assert_finite_sections(self, prepared, report):
        origin, axes = frame(prepared[0])
        mesh = report["surface"]
        domain = shapely.coverage_union_all(
            shapely.polygons(mesh.vertices[mesh.triangles, :2])
        )
        self.assertTrue(
            shapely.coverage_is_valid(
                shapely.polygons(mesh.vertices[mesh.triangles, :2])
            )
        )
        for item in report["terminal_values"]:
            cap, port = item["source_terminal"], item["port"]
            target, target_axes = frame(cap["anchor_longitude_latitude"])
            at, basis = axes.T @ (target - origin), axes.T @ target_axes
            point = at + port["height_m"] * (axes.T @ target_axes[:, 2])
            corners = mesh.vertices[list(cap["cap_vertices"])]
            own = (corners - at) @ basis
            segment = np.asarray(cap["source_segment"])
            projected = positions_in_frame(
                cap["anchor_longitude_latitude"], segment, np.zeros(2)
            )
            direction = projected[1, :2] - projected[0, :2]
            direction /= np.linalg.norm(direction)
            left = np.array([-direction[1], direction[0]])
            rounding = (
                64 * np.finfo(float).eps * max(1, abs(point).max(), abs(corners).max())
            )
            self.assertLessEqual(
                float(shapely.distance(domain, shapely.Point(point[:2]))), rounding
            )
            np.testing.assert_allclose(own[:, :2] @ direction, 0, atol=rounding, rtol=0)
            np.testing.assert_allclose(
                np.sort(own[[0, -1], :2] @ left), [-6, 6], atol=rounding, rtol=0
            )
            np.testing.assert_allclose(
                port["direction_EN"], direction, atol=1e-14, rtol=0
            )
            predicted = port["height_m"] + own[:, :2] @ np.asarray(port["plane_EN"])
            np.testing.assert_allclose(own[:, 2], predicted, atol=rounding, rtol=0)
            endpoints = set(cap["cap_vertices"][:2])
            touching = [
                face for face in mesh.triangles if endpoints <= set(map(int, face))
            ]
            self.assertTrue(touching)
            triangle = mesh.vertices[touching[0]]
            normal = (
                np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0]) @ basis
            )
            np.testing.assert_allclose(
                -normal[:2] / normal[2], port["plane_EN"], atol=1e-10, rtol=0
            )
            self.assertLessEqual(
                abs(port["longitudinal_grade"]), 0.04500000178813934 + 1e-9
            )
            self.assertLessEqual(abs(np.tan(port["crossfall_rad"])), 0.025 + 1e-9)

    def test_actual_both_terminals_and_whole_quality_affine_and_c1(self):
        nodes, spans, unknown = (
            self.source.nodes,
            self.source.spans,
            self.source.unknown_nodes,
        )
        for method in ("auto", "cone"):
            with self.subTest(method=method):
                prepared, reason, report = self.derive(method=method)
                self.assertEqual(reason, "")
                self.assertEqual(report["state"], "QualifiedTerminalField")
                self.assertEqual(len(report["terminal_values"]), 2)
                self.assertEqual(report["qualified_world_joins"], 0)
                self.assertEqual(report["terrain_samples"], 30)
                checked = report["terminal_quality"]
                self.assertTrue(checked["geometry_complete"])
                self.assertLessEqual(
                    checked["maximum_normalized_continuous_grade"], 1.0000001
                )
                self.assertLessEqual(
                    checked["maximum_normalized_finite_grade"], 1.0000001
                )
                self.assertLessEqual(checked["maximum_height_chord_bound_m"], 0.01)
                self.assertLessEqual(
                    checked["finite_contacts"]["maximum_finite_contact_grade_jump"],
                    0.005,
                )
                self.assertLessEqual(
                    checked["maximum_original_prior_vertex_error_m"], 3
                )
                self.assertEqual(report["terminal_work"]["port_queries"], 2)
                self.assert_finite_sections(prepared, report)
                self.assertFalse(report["surface"].vertices.flags.writeable)
                self.assertFalse(report["surface"].triangles.flags.writeable)
                if hasattr(prepared[1], "plan"):
                    self.assertIs(report["surface"], prepared[1].plan.mesh)
        self.assertIs(self.source.nodes, nodes)
        self.assertIs(self.source.spans, spans)
        self.assertIs(self.source.unknown_nodes, unknown)
        self.assertEqual(len(unknown), 3348)

    def test_request_order_and_larger_caps_preserve_values_mesh_and_binding(self):
        for method in ("auto", "cone"):
            a, ra, x = self.derive(method=method)
            b, rb, y = self.derive(
                method=method,
                seed=OTHER,
                budget=replace(PortBudget(), selected_spans=4096),
            )
            self.assertEqual((ra, rb), ("", ""))
            self.assertEqual(a[0], b[0])
            self.assertEqual(x["binding"], y["binding"])
            self.assertEqual(x["terminal_values"], y["terminal_values"])
            self.assertEqual(
                x["surface"].vertices.tobytes(), y["surface"].vertices.tobytes()
            )
            self.assertEqual(
                x["surface"].triangles.tobytes(), y["surface"].triangles.tobytes()
            )

    def test_new_recipe_is_bound_and_old_domain_query_has_no_global_bypass(self):
        old, reason, baseline = derive_scoped_ground_field(
            self.source,
            SEED,
            self.groups,
            self.ways,
            self.terrain,
            source_binding=self.source.binding,
            candidate_binding=self.binding,
            terrain_binding=self.terrain_binding,
            maximum_prior_change_m=3,
            scope_index=self.index,
        )
        self.assertEqual(reason, "")
        prepared, reason, report = self.derive()
        self.assertEqual(reason, "")
        self.assertNotEqual(report["binding"], baseline["binding"])
        self.assertEqual(
            report["binding"]["procedure"]["terminal_recipe"],
            "outshine/source-terminal-sections/1",
        )
        np.testing.assert_array_equal(
            report["source_input"].surface.vertices,
            baseline["source_input"].surface.vertices,
        )
        cap = report["terminal_values"][0]["source_terminal"]
        with self.assertRaisesRegex(ValueError, "leaves bounded original"):
            canonical_port(
                old[1],
                old[0],
                cap["anchor_longitude_latitude"],
                cap["source_segment"],
                cap["half_width_m"],
                float(self.terrain([cap["anchor_longitude_latitude"]])[0]),
            )
        with self.assertRaisesRegex(ValueError, "leaves bounded original"):
            prepared[1].evaluate(np.array([1e5, 1e5]))

    def test_historical_half_metre_recipe_stays_pending(self):
        prepared, reason, report = self.derive(limit=0.5)
        self.assertIsNone(prepared)
        self.assertEqual(reason, "ground_field_unavailable:ground_prior_error_limit")
        self.assertEqual(report["state"], "Pending")

    def test_missing_terminal_and_index_mismatch_remain_before_dem(self):
        group = next(g for g in self.groups if g.identifier.endswith(":1152"))
        changed = replace(
            group,
            members=(group.members[0], replace(group.members[1], source_terminal=None)),
        )
        groups = tuple(
            changed if g.identifier == group.identifier else g for g in self.groups
        )
        index, reason, _ = prepare_ground_scope_index(
            self.source,
            groups,
            self.ways,
            source_binding=self.source.binding,
            candidate_binding=self.binding,
        )
        self.assertEqual(reason, "")
        sample = Mock(side_effect=AssertionError("no DEM for unqualified facts"))
        prepared, reason, report = self.derive(
            groups=groups, scope_index=index, terrain=sample
        )
        self.assertIsNone(prepared)
        self.assertEqual(reason, "unqualified_ground_scope_exit_terminal")
        self.assertEqual(report["terrain_samples"], 0)
        sample.assert_not_called()
        prepared, reason, report = self.derive(groups=self.groups[::-1], terrain=sample)
        self.assertIsNone(prepared)
        self.assertEqual(reason, "ground_scope_index_binding_mismatch")
        sample.assert_not_called()

    def test_query_time_and_geometry_budgets_stay_bounded(self):
        budgets = [
            replace(PortBudget(), requests=1),
            replace(PortBudget(), surface=replace(PortBudget().surface, max_queries=1)),
            replace(
                PortBudget(), surface=replace(PortBudget().surface, time_limit_s=0)
            ),
            replace(
                PortBudget(),
                surface=replace(PortBudget().surface, geometry_time_limit_s=0),
            ),
            replace(
                PortBudget(),
                surface=replace(PortBudget().surface, max_output_vertices=4),
            ),
        ]
        for budget in budgets:
            sample = Mock(
                side_effect=AssertionError("no DEM for known insufficient budget")
            )
            prepared, reason, report = self.derive(budget=budget, terrain=sample)
            self.assertIsNone(prepared)
            self.assertTrue(reason)
            self.assertEqual(report["terrain_samples"], 0)
            sample.assert_not_called()
        prepared, reason, report = self.derive(
            budget=replace(
                PortBudget(), surface=replace(PortBudget().surface, max_queries=2)
            )
        )
        self.assertEqual(reason, "")
        self.assertEqual(report["terminal_work"]["port_queries"], 2)
        with self.assertRaisesRegex(ValueError, "query budget"):
            prepared[1].evaluate(report["surface"].vertices[0, :2])

    def test_existing_default_query_limit_is_preserved(self):
        mesh = Mesh(
            np.array([[0.0, 0, 0], [1, 0, 0], [0, 1, 0]]), np.array([[0, 1, 2]])
        )
        old, limited = (
            CoarseDomain(mesh, np.array([0])),
            CoarseDomain(mesh, np.array([0]), max_queries=1),
        )
        self.assertEqual(old.max_queries, 4096)
        self.assertEqual(limited.part_at(np.array([0.25, 0.25])), 0)
        with self.assertRaisesRegex(ValueError, "query budget"):
            limited.part_at(np.array([0.25, 0.25]))
        self.assertEqual(limited.face_bounds_tests, 1)

    def test_subdivided_cap_keeps_its_original_face_provenance(self):
        prepared, reason, report = derive_scoped_ground_field(
            self.source,
            SEED,
            self.groups,
            self.ways,
            self.terrain,
            source_binding=self.source.binding,
            candidate_binding=self.binding,
            terrain_binding=self.terrain_binding,
            maximum_prior_change_m=3,
            method="cone",
            scope_index=self.index,
        )
        self.assertEqual(reason, "")
        plan = prepared[1].plan
        a, b = map(int, plan.original_vertex_map[[0, 1]])
        face = next(
            i
            for i, row in enumerate(plan.mesh.triangles)
            if {a, b} <= set(map(int, row))
        )
        row = list(map(int, plan.mesh.triangles[face]))
        i = next(i for i in range(3) if {row[i], row[(i + 1) % 3]} == {a, b})
        a, b, c = row[i], row[(i + 1) % 3], row[(i + 2) % 3]
        vertex = len(plan.mesh.vertices)
        vertices = np.vstack(
            (plan.mesh.vertices, (plan.mesh.vertices[a] + plan.mesh.vertices[b]) / 2)
        )
        faces = np.vstack((plan.mesh.triangles, [vertex, b, c]))
        faces[face] = [a, vertex, c]
        arrays = {
            name: np.concatenate((getattr(plan, name), getattr(plan, name)[[face]]))
            for name in (
                "face_parts",
                "grade_frames",
                "source_faces",
                "height_error_bounds_m",
                "parameter_intervals",
            )
        }
        new = replace(
            plan,
            mesh=Mesh(vertices, faces),
            targets_m=np.r_[
                plan.targets_m, (plan.targets_m[a] + plan.targets_m[b]) / 2
            ],
            **arrays,
        )
        prepared[1].plan = new
        report.update(surface=new.mesh, surface_source_faces=new.source_faces)
        mesh, caps, work = rebuild_terminal_caps(prepared, report, self.index)
        self.assertEqual(len(caps[0]["cap_vertices"]), 3)
        self.assertIn(vertex, caps[0]["cap_vertices"])
        self.assertEqual(len(mesh.triangles), len(plan.mesh.triangles) + 1)
        self.assertEqual(work["cap_vertices"], 5)


if __name__ == "__main__":
    unittest.main()
