"""Actual complete caps, frames, widths and explicit missing-authority controls."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from bound_bridge_end_surface import build_end_surface
from bridge_end_surface_geometry import end_geometry
from bridge_end_surface_inputs import actual_end_inputs
from outshine_ref.contracts import Mesh
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._geodesy import frame, positions_in_frame
from outshine_ref.transport.port_heights._world import normal_gradients


class ActualBridgeEndSurfaces(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source, cls.index, cls.prepared, cls.report, _ = actual_end_inputs()
        cls.groups = [
            v["source_terminal"]["group"]
            for v in cls.report["terminal_values"]
            if any(
                cls.index.index[1][m.edge[:2]].bridge
                for m in cls.index.groups_by_node[
                    v["source_terminal"]["source_node"]
                ].members
            )
        ]
        cls.outputs = {
            g: build_end_surface(cls.prepared, cls.report, cls.index, g)
            for g in cls.groups
        }

    def build(self, *, report=None, index=None, prepared=None, group=None, budget=None):
        return build_end_surface(
            self.prepared if prepared is None else prepared,
            self.report if report is None else report,
            self.index if index is None else index,
            self.groups[0] if group is None else group,
            budget=budget,
        )

    def test_actual_two_groups_use_different_original_bridges(self):
        self.assertEqual(len(self.outputs), 2)
        ids = [r["bridge_member"][0] for _, r in self.outputs.values()]
        self.assertEqual(len(set(ids)), 2)
        for mesh, r in self.outputs.values():
            self.assertEqual((len(mesh.vertices), len(mesh.triangles)), (22, 20))
            self.assertEqual(r["qualified_contacts"], 0)
            self.assertEqual(r["qualified_world_joins"], 0)
            self.assertIn("whole_bridge", r["pending"])
            self.assertEqual(r["work"]["additional_DEM_samples"], 0)

    def test_whole_original_cap_is_byte_identical_and_c1_to_actual_ground_face(self):
        ground = self.report["surface"]
        gg = normal_gradients(
            ground.vertices[:, :2], ground.triangles, ground.vertices[:, 2]
        )
        for mesh, r in self.outputs.values():
            cap = list(r["near_ground_vertices"])
            self.assertEqual(
                mesh.vertices[:2].tobytes(), ground.vertices[cap].tobytes()
            )
            ids = np.flatnonzero(np.isin(ground.triangles, cap).sum(axis=1) == 2)
            self.assertEqual(len(ids), 1)
            gradient = normal_gradients(
                mesh.vertices[:, :2], mesh.triangles, mesh.vertices[:, 2]
            )
            self.assertLess(
                float(np.linalg.norm(gradient - gg[ids[0]], axis=1).max()), 1e-8
            )

    def test_independent_ecef_transform_reconstructs_all_port_vertices(self):
        origin, axes = frame(self.prepared[0])
        for mesh, r in self.outputs.values():
            target, target_axes = frame(r["port_frame_anchor"])
            points = (origin + mesh.vertices @ axes.T - target) @ target_axes
            np.testing.assert_allclose(
                points, r["quality"]["port_vertices"], atol=3e-9, rtol=0
            )

    def test_widths_and_grades_hold_on_every_planar_axis_segment(self):
        for _, r in self.outputs.values():
            q = r["quality"]
            self.assertAlmostEqual(q["minimum_width_m"], 1.5, places=8)
            self.assertAlmostEqual(q["maximum_width_m"], 1.5, places=8)
            self.assertLess(
                float(abs(q["longitudinal_grade_by_axis_segment"]).max()), 0.2
            )
            self.assertLess(float(abs(q["cross_grade_by_axis_segment"]).max()), 0.025)
            self.assertLessEqual(
                q["nominal_end_length_m"], q["first_span_length_m"] / 4
            )

    def test_reverse_request_order_and_larger_caps_keep_meshes(self):
        budget = PortBudget()
        budget = replace(
            budget,
            surface=replace(
                budget.surface,
                max_output_vertices=2 * budget.surface.max_output_vertices,
            ),
        )
        for group in self.groups[::-1]:
            actual, _ = self.build(group=group, budget=budget)
            expected, _ = self.outputs[group]
            np.testing.assert_array_equal(actual.vertices, expected.vertices)
            np.testing.assert_array_equal(actual.triangles, expected.triangles)

    def test_reflected_helper_direction_cannot_enter_actual_ground(self):
        # Reverse only the internal direction query, not captured Source facts.
        def reflected(*args, **kwargs):
            return -positions_in_frame(*args, **kwargs)

        for group in self.groups:
            with (
                self.subTest(group=group),
                patch("bridge_end_surface_geometry.positions_in_frame", reflected),
                self.assertRaisesRegex(ValueError, "bridge_entry_direction_recipe"),
            ):
                self.build(group=group)

    def test_cap_input_order_keeps_outward_surface(self):
        def swapped(anchor, port, near, *args, **kwargs):
            original, row = end_geometry(anchor, port, near, *args, **kwargs)
            mesh, _ = end_geometry(anchor, port, near[::-1], *args, **kwargs)
            np.testing.assert_array_equal(mesh.vertices, original.vertices)
            np.testing.assert_array_equal(mesh.triangles, original.triangles)
            return original, row

        for group in self.groups:
            with patch("bound_bridge_end_surface.end_geometry", swapped):
                mesh, _ = self.build(group=group)
            original, _ = self.outputs[group]
            np.testing.assert_array_equal(mesh.vertices, original.vertices)
            np.testing.assert_array_equal(mesh.triangles, original.triangles)

    def test_missing_or_ambiguous_original_cap_face_is_rejected(self):
        ground = self.report["surface"]
        for group in self.groups:
            cap = self.outputs[group][1]["near_ground_vertices"]
            touches = np.isin(ground.triangles, cap).sum(axis=1) == 2
            for faces in (
                ground.triangles[~touches],
                np.vstack((ground.triangles, ground.triangles[touches])),
            ):
                with self.assertRaisesRegex(ValueError, "ground_cap_boundary"):
                    self.build(
                        group=group,
                        report={**self.report, "surface": Mesh(ground.vertices, faces)},
                    )

    def test_wrong_prepared_frame_is_rejected(self):
        anchor, field = self.prepared
        with self.assertRaisesRegex(ValueError, "ground_frame_binding"):
            self.build(prepared=((anchor[0] + 1e-4, anchor[1]), field))

    def test_missing_binding_and_stale_candidate_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "ground_field_binding"):
            self.build(report={"state": "Pending", "binding": None})
        with self.assertRaisesRegex(ValueError, "ground_field_binding"):
            self.build(
                index=replace(
                    self.index, candidate_binding={"source_facts_digest": "stale"}
                )
            )

    def test_missing_and_nonbridge_terminal_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "missing_qualified_ground_terminal"):
            self.build(group="missing")
        third = next(
            v["source_terminal"]["group"]
            for v in self.report["terminal_values"]
            if v["source_terminal"]["group"] not in self.groups
        )
        with self.assertRaises(ValueError):
            self.build(group=third)

    def test_tiny_output_and_zero_time_budgets_publish_no_surface(self):
        budget = PortBudget()
        for surface in (
            replace(budget.surface, max_output_vertices=21),
            replace(budget.surface, geometry_time_limit_s=0),
        ):
            with self.assertRaisesRegex(ValueError, "budget"):
                self.build(budget=replace(budget, surface=surface))

    def test_missing_profile_and_frontier_remain_rejected(self):
        _, row = self.outputs[self.groups[0]]
        node = row["ground_member"][2]
        group = self.index.groups_by_node[node]
        table = dict(self.index.groups_by_node)
        table[node] = replace(group, frontier=True)
        with self.assertRaisesRegex(ValueError, "source_frontier"):
            self.build(index=replace(self.index, groups_by_node=table))
        bridge = next(
            m for m in group.members if self.index.index[1][m.edge[:2]].bridge
        )
        ways = dict(self.index.ways_by_id)
        ways[bridge.source_way] = replace(
            ways[bridge.source_way], maximum_gradient_word=None
        )
        with self.assertRaisesRegex(ValueError, "missing_source_profile"):
            self.build(index=replace(self.index, ways_by_id=ways))

    def test_incomplete_group_is_rejected_before_geometry(self):
        _, row = self.outputs[self.groups[0]]
        node = row["ground_member"][2]
        table = dict(self.index.groups_by_node)
        table[node] = replace(table[node], complete=False)
        with (
            patch(
                "bound_bridge_end_surface.end_geometry",
                side_effect=AssertionError("invalid group reached geometry"),
            ),
            self.assertRaisesRegex(ValueError, "incomplete_original_incidence"),
        ):
            self.build(index=replace(self.index, groups_by_node=table))

    def test_ground_mesh_and_source_unknown_remain_unchanged(self):
        mesh = self.report["surface"]
        original = mesh.vertices.tobytes(), mesh.triangles.tobytes()
        unknown = self.source.unknown_nodes
        for group in self.groups:
            self.build(group=group)
        self.assertEqual(original, (mesh.vertices.tobytes(), mesh.triangles.tobytes()))
        self.assertEqual(unknown, self.source.unknown_nodes)


if __name__ == "__main__":
    unittest.main()
