"""Whole generated profiles, frozen XY regions and actual stair-body consumers."""

import copy
import math
import unittest

import numpy as np
from mvt_t_connection_inputs import inputs
from mvt_t_connections import plan_connections
from mvt_t_profile_audit import audit_profiles
from mvt_t_profile_layout import prepare_profiles
from mvt_t_profile_products import shared_products
from mvt_t_profile_solve import solve_profiles
from stair_junction_body import build
from stair_network_geometry import vertex_links


def prepared(*, mesh_recipe="outward_binary_grid"):
    case, sources = inputs()
    return prepare_profiles(
        case, sources, plan_connections(sources), mesh_recipe=mesh_recipe
    )


class SharedProfiles(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plan = prepared()
        cls.heights, cls.note = solve_profiles(cls.plan)
        if cls.heights is None:
            raise ValueError(cls.note)
        cls.body, cls.data, cls.geometry = build(cls.plan, cls.heights)

    def test_whole_surfaces_both_flat_landings_actual_treads_and_float32(self):
        proof = audit_profiles(self.plan, self.heights, self.data)
        self.assertLess(proof["maximum_prior_error_m"], 0.53)
        self.assertLess(proof["whole_landing_error_m"], 1e-7)
        self.assertLessEqual(max(proof["body"]["source_coverage_error_m2"]), 1e-7)
        self.assertGreater(proof["body"]["treads"]["risers"], 0)
        self.assertLessEqual(proof["body"]["treads"]["maximum_float32_riser_m"], 0.18)
        vertex_links(self.body.vertices, self.body.triangles)

    def test_xy_regions_are_prepared_before_heights_and_cover_retained_sources(self):
        plan = prepared()
        before = [d.wkb for d in plan["prepared_domains"]] + [
            m.wkb for m in plan["masks"]
        ]
        heights, note = solve_profiles(plan)
        self.assertIsNotNone(heights, note)
        self.assertEqual(
            before,
            [d.wkb for d in plan["prepared_domains"]] + [m.wkb for m in plan["masks"]],
        )
        self.assertEqual(len(plan["footprint_checks"]), 3)
        self.assertLess(max(plan["landing_displacement_m"]), 6 * 2**-11)

    def test_navigation_mesh_interfaces_and_terrain_share_one_plan(self):
        products = shared_products(self.plan, self.heights, self.data)
        _case, original = inputs()
        self.assertEqual(products["format_version"], 1)
        for source in original:
            metadata = products["source_metadata"][source["source_id"]]
            self.assertEqual(
                metadata["provider_feature_id"], source["provider_feature_id"]
            )
            self.assertEqual(metadata["tile"], source["tile"])
            self.assertEqual(metadata["supplied_tags"], source["supplied_tags"])
            self.assertEqual(metadata["xy_m"], source["xy_m"])
        self.assertEqual(len(products["landings"]), 2)
        self.assertEqual(len(products["terrain_reservations"]), 3)
        self.assertTrue(products["interface_ports"])
        for port in products["interface_ports"]:
            np.testing.assert_allclose(
                np.array(port["profile_xyz_m"])[:, 2],
                products["landings"][port["landing"]]["height_m"],
                atol=1e-7,
            )
        navigation = products["navigation"]
        adjacency = {i: set() for i in range(len(navigation["nodes_xyz_m"]))}
        for edge in navigation["edges"]:
            a, b = edge["nodes"]
            adjacency[a].add(b)
            adjacency[b].add(a)
        seen, pending = set(), [0]
        while pending:
            node = pending.pop()
            if node not in seen:
                seen.add(node)
                pending.extend(adjacency[node])
        self.assertEqual(len(seen), len(adjacency))
        self.assertEqual(len(products["external_contacts"]), 4)
        self.assertTrue(
            all(
                r["footprint_parts"][0]["outer_xy_m"]
                for r in products["terrain_reservations"]
            )
        )
        self.assertTrue(
            all(
                c["profile_status"].startswith("proposed")
                for c in products["external_contacts"]
            )
        )

    def test_independent_ipm_satisfies_same_finite_objective_and_body_requirements(
        self,
    ):
        heights, note = solve_profiles(self.plan, solver="highs-ipm")
        self.assertIsNotNone(heights, note)
        self.assertAlmostEqual(note["objective"], self.note["objective"], places=7)
        _body, data, _geometry = build(self.plan, heights)
        audit_profiles(self.plan, heights, data)

    def test_tight_prior_budget_is_not_relaxed_to_get_a_result(self):
        heights, note = solve_profiles(self.plan, max_prior_change_m=0.1)
        self.assertIsNone(heights)
        self.assertFalse(note["valid"])
        self.assertIn("infeasible", note["status"].lower())

    def test_unconditioned_source_mesh_does_not_bypass_float32_admission(self):
        with self.assertRaisesRegex(ValueError, "precision mesh"):
            solve_profiles(prepared(mesh_recipe="source"))

    def test_rebuilding_landing_masks_from_old_buffers_is_not_a_shared_plan(self):
        changed = dict(self.plan, masks=self.plan["input_landing_masks"])
        with self.assertRaisesRegex(ValueError, "landing"):
            audit_profiles(changed, self.heights, self.data)

    def test_missing_source_cap_and_changed_landing_are_rejected(self):
        data = dict(self.data, face_sources=self.data["face_sources"].copy())
        data["face_sources"][np.flatnonzero(data["face_sources"] & 2)[0]] &= ~2
        with self.assertRaises(ValueError):
            audit_profiles(self.plan, self.heights, data)
        heights = self.heights.copy()
        heights[self.plan["parts"][0][0][0]] += 0.1
        with self.assertRaises(ValueError):
            audit_profiles(self.plan, heights, self.data)

    def test_descending_source_orientation_keeps_valid_shared_stair(self):
        case, sources = inputs()
        case, sources = copy.deepcopy(case), copy.deepcopy(sources)
        owner = next(i for i, r in enumerate(sources) if r["kind"] == "steps")
        sources[owner]["xy_m"].reverse()
        case["ways"][owner]["sampled_design_xyz_m"].reverse()
        plan = prepare_profiles(
            case, sources, plan_connections(sources), mesh_recipe="outward_binary_grid"
        )
        heights, note = solve_profiles(plan)
        self.assertIsNotNone(heights, note)
        _body, data, _geometry = build(plan, heights)
        audit_profiles(plan, heights, data)
        self.assertEqual(plan["stair_directions"][owner], -1)

    def test_rotated_original_lines_keep_whole_body_and_shared_contacts(self):
        case, sources = inputs()
        case, sources = copy.deepcopy(case), copy.deepcopy(sources)
        angle = 0.43
        matrix = np.array(
            [[math.cos(angle), -math.sin(angle)], [math.sin(angle), math.cos(angle)]]
        )
        for way, source in zip(case["ways"], sources, strict=True):
            source["xy_m"] = (np.array(source["xy_m"]) @ matrix.T).tolist()
            prior = np.array(way["sampled_design_xyz_m"])
            prior[:, :2] = prior[:, :2] @ matrix.T
            way["sampled_design_xyz_m"] = prior.tolist()
        plan = prepare_profiles(
            case, sources, plan_connections(sources), mesh_recipe="outward_binary_grid"
        )
        heights, note = solve_profiles(plan)
        self.assertIsNotNone(heights, note)
        _body, data, _geometry = build(plan, heights)
        audit_profiles(plan, heights, data)
        shared_products(plan, heights, data)


if __name__ == "__main__":
    unittest.main()
