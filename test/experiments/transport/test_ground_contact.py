"""Whole fixed road/outer profiles and explicit local terrain failure controls."""

import unittest
from dataclasses import replace

import numpy as np
from ground_contact import ContactLimits, build_contact
from ground_contact_checks import edge_owners, qualify_contact
from ground_contact_inputs import load_context
from outshine_ref.contracts import Mesh


class ActualGroundContact(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.context = load_context()
        cls.result = build_contact(
            cls.context.road, cls.context.road_dem_z, cls.context.sample_z
        )

    def build(self, **options):
        return build_contact(
            self.context.road, self.context.road_dem_z, self.context.sample_z, **options
        )

    def test_all_xyz_faces_and_ninety_whole_interfaces_are_preserved(self):
        checked = qualify_contact(self.context.road, self.result)
        self.assertEqual(self.context.report["state"], "QualifiedTerminalField")
        self.assertEqual(len(self.context.report["terminal_values"]), 3)
        self.assertEqual(checked["road_vertices_exact"], 123)
        self.assertEqual(checked["road_faces_exact"], 154)
        self.assertEqual(checked["whole_road_soil_edges"], 90)
        self.assertEqual(checked["whole_shared_edge_height_gap_m"], 0)
        self.assertGreater(checked["maximum_before_rim_dem_gap_m"], 1.8)
        self.assertLessEqual(checked["maximum_soil_grade"], (2 / 3) * 1.0000001)
        self.assertEqual(checked["outer_xyz_exact"], 4)

    def test_general_union_control_produces_same_actual_qualified_terrain(self):
        other = self.build(method="general")
        self.assertEqual(
            qualify_contact(self.context.road, other),
            qualify_contact(self.context.road, self.result),
        )
        np.testing.assert_array_equal(
            other["mesh"].vertices, self.result["mesh"].vertices
        )
        np.testing.assert_array_equal(
            other["mesh"].triangles, self.result["mesh"].triangles
        )

    def test_five_metre_fixed_height_witness_rejects_before_mesh_work(self):
        result = self.build(limits=ContactLimits(margins_m=(5.0,)))
        self.assertEqual(result["status"], "pending")
        candidate = result["candidates"][0]
        self.assertEqual(candidate["reason"], "fixed_corner_lipschitz_conflict")
        self.assertGreater(candidate["corner_height_witness"]["excess_m"], 2)
        self.assertNotIn("work", candidate)

    def test_ten_metre_cell_failure_does_not_claim_global_infeasibility(self):
        result = self.build(limits=ContactLimits(margins_m=(10.0,)))
        self.assertEqual(result["status"], "pending")
        self.assertEqual(result["candidates"][0]["reason"], "fixed_cell_grade_conflict")
        self.assertGreater(result["candidates"][0]["maximum_soil_grade"], 2 / 3)

    def test_input_budget_prevents_terrain_queries_and_keeps_road_unchanged(self):
        before = self.context.road.vertices.tobytes()

        def forbidden(_):
            self.fail("budget-rejected contact queried terrain")

        for limits in (
            ContactLimits(maximum_road_vertices=10),
            ContactLimits(maximum_faces=10),
        ):
            result = build_contact(
                self.context.road, self.context.road_dem_z, forbidden, limits=limits
            )
            self.assertEqual(result["reason"], "road_contact_input_budget")
        self.assertEqual(before, self.context.road.vertices.tobytes())

    def test_missing_dem_does_not_release_an_outer_profile(self):
        result = build_contact(
            self.context.road,
            self.context.road_dem_z,
            lambda xy: np.full(len(xy), np.nan),
        )
        self.assertEqual(result["status"], "pending")
        self.assertTrue(
            all(
                c["reason"] == "missing_or_invalid_outer_dem"
                for c in result["candidates"]
            )
        )
        self.assertNotIn("mesh", result)

    def test_zero_refinement_budget_retains_a_whole_valid_coarse_contact(self):
        result = self.build(limits=ContactLimits(maximum_new_vertices=0))
        checked = qualify_contact(self.context.road, result)
        self.assertEqual(checked["whole_road_soil_edges"], 90)
        self.assertEqual(result["refinement"]["new_vertices"], 0)
        self.assertEqual(result["refinement"]["reason"], "refinement_budget_exhausted")

    def test_point_contact_values_do_not_substitute_for_full_segment_coverage(self):
        mesh = self.result["mesh"]
        faces = np.delete(mesh.triangles, len(self.context.road.triangles), axis=0)
        broken = self.result | {"mesh": Mesh(mesh.vertices, faces)}
        np.testing.assert_array_equal(
            broken["mesh"].vertices[self.result["road_vertex_map"]],
            self.context.road.vertices,
        )
        with self.assertRaises(ValueError):
            qualify_contact(self.context.road, broken)

    def test_a_soil_only_t_junction_is_rejected_with_all_road_points_retained(self):
        mesh = self.result["mesh"]
        count = len(self.context.road.triangles)
        owners = edge_owners(mesh.triangles)
        edge, faces = next(
            (e, f)
            for e, f in owners.items()
            if len(f) == 2 and sum(i < count for i in f) == 1
        )
        owner = next(i for i in faces if i >= count)
        face = mesh.triangles[owner]
        first = next(i for i in range(3) if set(face[[i, (i + 1) % 3]]) == set(edge))
        a, b, c = map(int, face[[first, (first + 1) % 3, (first + 2) % 3]])
        at = len(mesh.vertices)
        vertices = np.vstack((mesh.vertices, (mesh.vertices[a] + mesh.vertices[b]) / 2))
        triangles = np.vstack((mesh.triangles, np.array([[at, b, c]])))
        triangles[owner] = (a, at, c)
        broken = self.result | {"mesh": Mesh(vertices, triangles)}
        with self.assertRaisesRegex(ValueError, "nonconforming"):
            qualify_contact(self.context.road, broken)

    def test_changing_a_fixed_outer_xyz_is_not_hidden_by_point_contacts(self):
        mesh = self.result["mesh"]
        vertices = mesh.vertices.copy()
        vertices[self.result["outer_vertex_ids"][0], 2] += 0.01
        with self.assertRaisesRegex(ValueError, "outer_xyz_changed"):
            qualify_contact(
                self.context.road,
                self.result | {"mesh": Mesh(vertices, mesh.triangles)},
            )


class ContactParameters(unittest.TestCase):
    def test_invalid_capacities_and_variant_families_are_rejected(self):
        for values in (
            {"margins_m": (10, 5)},
            {"margins_m": (float("nan"),)},
            {"maximum_new_vertices": -1},
            {"soil_grade": 0},
            {"maximum_faces": 0},
        ):
            with self.assertRaises(ValueError):
                ContactLimits(**values)
        self.assertEqual(
            replace(ContactLimits(), maximum_new_vertices=0).maximum_new_vertices, 0
        )


if __name__ == "__main__":
    unittest.main()
