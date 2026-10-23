"""Complete owned networks: Float32 bodies, qualified navigation and shared terrain."""

import copy
import unittest

import numpy as np
from mvt_independent_flight_body import construct
from mvt_independent_flight_inputs import inputs, prepared
from mvt_t_connection_inputs import inputs as original_inputs
from mvt_t_profile_products import shared_products
from mvt_t_profile_solve import solve_profiles
from stair_junction_body import build


def navigation_components(navigation):
    neighbours = {i: set() for i in range(len(navigation["nodes_xyz_m"]))}
    for edge in navigation["edges"]:
        a, b = edge["nodes"]
        neighbours[a].add(b)
        neighbours[b].add(a)
    result, remaining = [], set(neighbours)
    while remaining:
        reached, todo = set(), [min(remaining)]
        while todo:
            node = todo.pop()
            if node not in reached:
                reached.add(node)
                todo.extend(neighbours[node] - reached)
        result.append(reached)
        remaining -= reached
    return result


class IndependentFlightTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = {name: construct(name) for name in ("stacked", "crossed")}

    def test_two_declared_flights_keep_all_eight_profiles_and_positive_float32_bodies(
        self,
    ):
        for name, case in self.cases.items():
            with self.subTest(name=name):
                self.assertEqual(len(case["plan"]["stairs"]), 2)
                self.assertEqual(len(case["contacts"]), 8)
                self.assertEqual(len(case["plan"]["rows"]), 6)
                self.assertEqual(
                    case["float32_orientation"]["invalid_float32_faces"], 0
                )
                self.assertTrue(all(v > 0 for v in case["float32_body_volumes_m3"]))
                self.assertGreater(case["global_vertical_clearance_bound_m"], 2)
                self.assertLess(
                    max(case["fixed_profiles"]["height_errors_m"].values()), 1e-10
                )
                self.assertEqual(len(case["products"]["external_contacts"]), 8)

    def test_navigation_has_two_connected_owned_networks_even_at_identical_xy(self):
        for name, case in self.cases.items():
            with self.subTest(name=name):
                nav = case["products"]["navigation"]
                groups = navigation_components(nav)
                self.assertEqual(sorted(map(len, groups)), [7, 7])
                upper = set(
                    case["plan"]["controlled_derivation"]["upper_source_mapping"]
                )
                for group in groups:
                    sources = {
                        e["source_id"] for e in nav["edges"] if set(e["nodes"]) <= group
                    }
                    self.assertEqual(len(sources), 3)
                    self.assertTrue(sources <= upper or not sources & upper)
        nodes = np.asarray(
            self.cases["stacked"]["products"]["navigation"]["nodes_xyz_m"]
        )
        self.assertEqual(len(np.unique(nodes[:, :2], axis=0)), 7)
        self.assertEqual(len(nodes), 14)

    def test_each_navigation_stair_uses_its_actual_body_level_slice(self):
        for case in self.cases.values():
            data, plan = case["data"], case["plan"]
            levels = case["products"]["navigation"]["stair_levels_by_source_m"]
            self.assertEqual(len(levels), 2)
            for owner, start, end in zip(
                data["stair_owners"],
                data["stair_level_offsets"][:-1],
                data["stair_level_offsets"][1:],
                strict=True,
            ):
                key = plan["rows"][owner]["source_id"]
                np.testing.assert_array_equal(levels[key], data["levels_m"][start:end])
                selected = ((data["face_sources"] & (1 << int(owner))) != 0) & (
                    data["face_step"] >= 0
                )
                actual = data["body_vertices"][data["top_faces"][selected], 2].astype(
                    np.float32
                )
                expected = data["levels_m"][data["face_step"][selected]].astype(
                    np.float32
                )
                np.testing.assert_array_equal(
                    actual, np.repeat(expected[:, None], 3, axis=1)
                )

    def test_terrain_indices_reference_the_same_solved_height_vertices(self):
        for case in self.cases.values():
            p, d = case["plan"], case["data"]
            hints = case["products"]["terrain_reservations"]
            self.assertEqual(len(hints), 6)
            for hint, faces, row in zip(hints, p["parts"], p["rows"], strict=True):
                self.assertEqual(hint["source_id"], row["source_id"])
                self.assertEqual(hint["height_surface_array"], "reference_vertices")
                np.testing.assert_array_equal(hint["surface_face_ids"], faces)
                np.testing.assert_array_equal(
                    d["reference_vertices"][faces, 2], case["heights"][faces]
                )

    def test_missing_loft_or_port_owner_is_refused_before_solver_and_consumers(self):
        case = self.cases["stacked"]
        plan = copy.deepcopy(case["solver_plan"])
        plan["stair_lofts"].pop()
        calls = (
            lambda: solve_profiles(plan),
            lambda: build(plan, case["solver_heights"]),
            lambda: shared_products(plan, case["solver_heights"], case["data"]),
        )
        for call in calls:
            with self.assertRaisesRegex(ValueError, "own complete loft"):
                call()
        plan = copy.deepcopy(case["solver_plan"])
        plan["stair_lofts"][-1]["start_node"] = 0
        with self.assertRaisesRegex(ValueError, "ports differ"):
            solve_profiles(plan)

    def test_derived_instance_keeps_original_provider_facts_and_all_full_profiles(self):
        original, _sources = original_inputs()
        case, sources, proposal, native, derivation = inputs()
        self.assertEqual(len({s["source_id"] for s in sources}), 6)
        self.assertEqual(len(proposal["connections"]), 4)
        self.assertEqual(len(native["cuts"]), 8)
        self.assertEqual(case["ways"][:3], original["ways"])
        self.assertIn("not_additional_provider_evidence", derivation["kind"])
        for lower, upper in zip(case["ways"][:3], case["ways"][3:], strict=True):
            self.assertEqual(lower["tags"], upper["tags"])
            self.assertEqual(lower["source_xy_m"], upper["source_xy_m"])
            a, b = (
                np.asarray(lower["sampled_design_xyz_m"]),
                np.asarray(upper["sampled_design_xyz_m"]),
            )
            np.testing.assert_array_equal(a[:, :2], b[:, :2])
            np.testing.assert_array_equal(a[:, 2] + 10, b[:, 2])
        for lower, upper in zip(native["cuts"][:4], native["cuts"][4:], strict=True):
            self.assertEqual(lower["native_way"], upper["derived_from_native_way"])
            self.assertEqual(len(lower["triangles"]), len(upper["triangles"]))
            for a, b in zip(lower["triangles"], upper["triangles"], strict=True):
                expected = np.asarray(a["enu_m"]) + [0, 0, 10]
                np.testing.assert_array_equal(expected, b["enu_m"])

    def test_inverted_or_missing_height_order_does_not_discard_any_source(self):
        p, contacts = prepared()
        upper = set(p["controlled_derivation"]["upper_source_mapping"])
        for row in p["rows"]:
            row["separation_below_source_ids"] = (
                [] if row["source_id"] in upper else sorted(upper)
            )
        heights, note = solve_profiles(
            p,
            fixed_contacts=contacts,
            max_prior_change_m=3,
            gradient_model="axis_crossfall",
        )
        self.assertIsNone(heights)
        self.assertFalse(note["valid"])
        self.assertEqual(len(p["rows"]), 6)
        for row in p["rows"]:
            row.pop("separation_below_source_ids", None)
        with self.assertRaisesRegex(ValueError, "unplanned non-contact overlap"):
            solve_profiles(
                p,
                fixed_contacts=contacts,
                max_prior_change_m=3,
                gradient_model="axis_crossfall",
            )

    def test_rigid_controls_keep_all_profiles_and_both_navigable_levels(self):
        for name in ("stacked", "crossed"):
            with self.subTest(name=name):
                case = construct(name, global_pose=(0.437, (37, -81)))
                self.assertEqual(
                    case["float32_orientation"]["invalid_float32_faces"], 0
                )
                self.assertEqual(
                    sorted(
                        map(len, navigation_components(case["products"]["navigation"]))
                    ),
                    [7, 7],
                )
                self.assertLess(
                    max(case["fixed_profiles"]["height_errors_m"].values()), 1e-10
                )


if __name__ == "__main__":
    unittest.main()
