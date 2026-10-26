"""Actual sloped World profiles, indexed body and opaque source/port bindings."""

import copy
import json
import unittest

import numpy as np
from compare_mvt_native_body import orientation_counts
from outshine_ref.contracts import Mesh
from stair_network_geometry import vertex_links
from surface_solid_checks import audit_closed
from world_stair_reference import FIXTURE, construct, prepare


class CapturedWorldStair(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.capture = json.loads(FIXTURE.read_text())
        cls.case = construct(cls.capture)

    def test_every_exterior_knot_retains_its_complete_float32_word(self):
        case = self.case
        actual = case["data"]["body_vertices"].astype(np.float32)
        for port, expected in zip(case["ports"], case["profiles_xyz_m"], strict=True):
            np.testing.assert_array_equal(
                actual[port["body_vertex_ids"]].view(np.uint32),
                expected.astype(np.float32).view(np.uint32),
            )
            edges = set()
            for face in case["data"]["top_faces"]:
                edges.update(
                    tuple(sorted((int(a), int(b))))
                    for a, b in zip(face, np.roll(face, -1), strict=True)
                )
            for a, b in zip(
                port["body_vertex_ids"][:-1], port["body_vertex_ids"][1:], strict=True
            ):
                self.assertIn(tuple(sorted((a, b))), edges)

    def test_sloped_profiles_cannot_be_replaced_with_constant_landing_heights(self):
        profiles = self.case["profiles_xyz_m"]
        for profile in profiles:
            # This is the exact minimax lower bound for any constant replacement.
            self.assertGreater(np.ptp(profile[:, 2]) / 2, 0.01)
        self.assertGreater(np.ptp(profiles[1, :, 2]) / 2, 0.06)

    def test_both_transitions_use_the_owned_gate_height_deviation(self):
        plan, recipe = self.case["plan"], self.case["recipe"]
        np.testing.assert_array_equal(recipe["landing_lengths_m"], [0.75, 0.75])
        self.assertGreaterEqual(recipe["flight_run_m"] / recipe["risers"], 0.24)
        self.assertLessEqual(float(np.diff(self.case["data"]["levels_m"]).max()), 0.179)
        for ids, port in zip(
            (range(4, 8), range(8, 12)), self.capture["ports"], strict=True
        ):
            np.testing.assert_array_equal(
                self.case["heights"][list(ids)], port["gate"][2]
            )
        self.assertEqual(plan["stairs"], [(1, 0, 1)])

    def test_actual_float32_body_is_closed_positive_and_vertex_manifold(self):
        case = self.case
        self.assertEqual(orientation_counts(case["data"])["invalid_float32_faces"], 0)
        mesh = Mesh(
            case["body"].vertices.astype(np.float32).astype(float),
            case["body"].triangles,
        )
        self.assertGreater(audit_closed(mesh), 0)
        vertex_links(mesh.vertices, mesh.triangles)
        stepped = case["data"]["face_step"] >= 0
        top = case["data"]["body_vertices"][case["data"]["top_faces"][stepped], 2]
        self.assertTrue(np.all(np.ptp(top, axis=1) == 0))
        self.assertTrue(np.any(case["data"]["wall_riser"] == 1))

    def test_parts_keep_one_parent_and_original_unknown_source_facts(self):
        case = self.case
        self.assertEqual(case["capture"], self.capture)
        self.assertTrue(all(v is None for v in self.capture["placement"].values()))
        rows = case["plan"]["rows"]
        self.assertEqual(len({r["source_id"] for r in rows}), 3)
        self.assertEqual(len({r["parent_source_id"] for r in rows}), 1)
        self.assertEqual(
            [r["part_role"] for r in rows],
            ["start_transition", "flight", "end_transition"],
        )
        np.testing.assert_array_equal(
            np.unique(case["data"]["face_sources"]), [1, 2, 4]
        )

    def test_large_world_node_ids_never_pass_through_float64(self):
        actual = json.loads(json.dumps(self.case["ports"]))
        for port, expected in zip(
            actual, self.capture["end_world_node_ids"], strict=True
        ):
            self.assertIs(type(port["world_node_id"]), int)
            self.assertEqual(port["world_node_id"], expected)
            self.assertGreater(expected, 2**53)
            self.assertNotEqual(int(float(expected)), expected)

    def test_reverse_source_order_preserves_the_same_physical_body_and_ports(self):
        capture = copy.deepcopy(self.capture)
        for key in (
            "stations_east_north_grade_node",
            "ports",
            "reference_boundary_vertex_indices",
            "end_world_node_ids",
        ):
            capture[key].reverse()
        actual = construct(capture)
        for key in self.case["data"]:
            np.testing.assert_array_equal(actual["data"][key], self.case["data"][key])
        self.assertEqual(
            [p["world_node_id"] for p in actual["ports"]],
            self.capture["end_world_node_ids"],
        )

    def test_unfit_geometry_and_bad_id_are_local_failures_without_input_changes(self):
        before = copy.deepcopy(self.capture)
        construct(self.capture)
        self.assertEqual(self.capture, before)
        for edit in ("width", "axis", "node", "run"):
            capture = copy.deepcopy(self.capture)
            if edit == "width":
                capture["ports"][0]["gate"][5] *= 2
            elif edit == "axis":
                capture["stations_east_north_grade_node"][1][0] += 0.2
            elif edit == "node":
                capture["end_world_node_ids"][0] = float(
                    capture["end_world_node_ids"][0]
                )
            else:
                capture["ports"][1]["gate"][2] += 20
            saved = copy.deepcopy(capture)
            with self.subTest(edit=edit), self.assertRaises(ValueError):
                prepare(capture)
            self.assertEqual(capture, saved)


if __name__ == "__main__":
    unittest.main()
