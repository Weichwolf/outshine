"""Whole profiles, explicit ownership and actual native-point counterexamples."""

import copy
import unittest

import numpy as np
import shapely
from mvt_t_connection_inputs import inputs
from mvt_t_connections import plan_connections
from mvt_t_profile_audit import audit_profiles
from mvt_t_profile_contacts import (
    audit_fixed_contacts,
    contact_residuals,
    prepare_contacts,
)
from mvt_t_profile_layout import prepare_profiles
from mvt_t_profile_products import shared_products
from mvt_t_profile_solve import solve_profiles
from stair_junction_body import build


def contact(key, source, xyz):
    return {
        "contact_id": key,
        "source_id": source,
        "xyz_m": xyz,
        "binding": "fixed",
        "provenance": "explicit test declaration, not a native boundary qualification",
    }


def toy():
    xy = np.array([[0, 0], [1, 0], [2, 0], [0, 1], [1, 1], [2, 1]], dtype=float)
    faces = np.array([[0, 1, 3], [1, 4, 3], [1, 2, 4], [2, 5, 4]])
    return {
        "xy": xy,
        "parts": [faces],
        "rows": [{"source_id": "part-a"}],
        "prepared_domains": [shapely.box(0, 0, 2, 1)],
    }


class WholeFixedProfiles(unittest.TestCase):
    def test_interior_facet_peak_is_not_hidden_by_matching_endpoints(self):
        plan = toy()
        pin = contact("whole", "part-a", [[0, 0.5, 0], [2, 0.5, 0]])
        prepared, _work = prepare_contacts(plan, [pin])
        heights = np.array([0, 1, 0, 0, 1, 0], dtype=float)
        self.assertEqual(contact_residuals(prepared, heights)["whole"], 1)
        with self.assertRaisesRegex(ValueError, "mismatch"):
            audit_fixed_contacts(plan, heights, [pin])

    def test_profile_knot_inside_one_triangle_is_retained(self):
        plan = toy()
        pin = contact("kink", "part-a", [[0.1, 0.1, 0], [0.2, 0.1, 1], [0.3, 0.1, 0]])
        prepared, _work = prepare_contacts(plan, [pin])
        self.assertIn(1, prepared[0]["target_z_m"])
        self.assertGreater(contact_residuals(prepared, np.zeros(6))["kink"], 0.99)

    def test_short_representable_profile_has_no_metric_length_cutoff(self):
        plan = toy()
        pin = contact("short", "part-a", [[0.1, 0.5, 0], [0.1 + 1e-11, 0.5, 0]])
        prepared, work = prepare_contacts(plan, [pin])
        self.assertGreater(work["samples"], 2)
        self.assertEqual(contact_residuals(prepared, np.zeros(6))["short"], 0)

    def test_affine_barycentric_contact_equals_independent_surface_normals(self):
        plan = toy()
        heights = 2 * plan["xy"][:, 0] - 3 * plan["xy"][:, 1] + 4
        pin = contact("affine", "part-a", [[0, 0.5, 2.5], [2, 0.5, 6.5]])
        prepared, _work = prepare_contacts(plan, [pin])
        self.assertLess(contact_residuals(prepared, heights)["affine"], 1e-12)
        self.assertLess(
            audit_fixed_contacts(plan, heights, [pin])["height_errors_m"]["affine"],
            1e-12,
        )

    def test_scan_and_index_produce_identical_complete_constraints(self):
        plan = toy()
        pins = [contact("p", "part-a", [[0.1, 0.4, 0], [1.9, 0.4, 0]])]
        indexed, iw = prepare_contacts(plan, pins)
        scanned, sw = prepare_contacts(plan, pins, method="scan")
        for key in ("faces", "xy", "weights", "target_z_m"):
            np.testing.assert_array_equal(indexed[0][key], scanned[0][key])
        self.assertLess(iw["candidate_faces"], sw["candidate_faces"])

    def test_declared_part_owns_contact_even_when_another_layer_overlaps(self):
        plan = toy()
        plan["xy"] = np.vstack([plan["xy"], plan["xy"]])
        plan["parts"].append(plan["parts"][0] + 6)
        plan["rows"].append({"source_id": "part-b"})
        plan["prepared_domains"].append(plan["prepared_domains"][0])
        pin = contact("upper", "part-b", [[0.5, 0.5, 10]])
        prepared, _work = prepare_contacts(plan, [pin])
        self.assertTrue((prepared[0]["faces"] >= 6).all())
        self.assertEqual(
            audit_fixed_contacts(plan, np.r_[np.zeros(6), np.full(6, 10)], [pin])[
                "height_errors_m"
            ]["upper"],
            0,
        )

    def test_unknown_owner_outside_hole_and_unfixed_proposals_refused(self):
        plan = toy()
        bad = [
            contact("unknown", "other", [[0.5, 0.5, 0]]),
            contact("outside", "part-a", [[3, 0.5, 0]]),
        ]
        proposed = contact("proposed", "part-a", [[0.5, 0.5, 0]])
        proposed["binding"] = "proposed"
        for pin in bad + [proposed]:
            with self.assertRaises(ValueError):
                prepare_contacts(plan, [pin])
        plan["prepared_domains"] = [
            plan["prepared_domains"][0].difference(shapely.box(0.8, 0.3, 1.2, 0.7))
        ]
        pin = contact("hole", "part-a", [[0.1, 0.5, 0], [1.9, 0.5, 0]])
        with self.assertRaises(ValueError):
            prepare_contacts(plan, [pin])
        with self.assertRaises(ValueError):
            audit_fixed_contacts(plan, np.zeros(6), [pin])

    def test_budget_refuses_entire_binding_and_duplicate_or_folded_inputs(self):
        plan = toy()
        pin = contact("p", "part-a", [[0.1, 0.5, 0], [1.9, 0.5, 0]])
        with self.assertRaisesRegex(ValueError, "budget"):
            prepare_contacts(plan, [pin], max_samples=1)
        with self.assertRaisesRegex(ValueError, "candidate budget"):
            prepare_contacts(plan, [pin], max_candidates=1)
        with self.assertRaises(ValueError):
            prepare_contacts(plan, [pin, pin])
        folded = contact(
            "loop", "part-a", [[0.1, 0.5, 0], [1.9, 0.5, 0], [0.1, 0.5, 1]]
        )
        with self.assertRaisesRegex(ValueError, "simple"):
            prepare_contacts(plan, [folded])


class RecordedNativeContacts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case, cls.sources = inputs()
        cls.plan = prepare_profiles(
            cls.case,
            cls.sources,
            plan_connections(cls.sources),
            mesh_recipe="outward_binary_grid",
        )
        cls.heights, cls.note = solve_profiles(cls.plan)
        if cls.heights is None:
            raise ValueError(cls.note)

    def native_contacts(self, way_id, ends):
        owner = next(
            i for i, w in enumerate(self.case["ways"]) if w["native_way"] == way_id
        )
        return [
            contact(
                f"native:{way_id}:{end}",
                self.sources[owner]["source_id"],
                [self.case["ways"][owner]["prepared_stations_xyz_m"][end]],
            )
            for end in ends
        ]

    def test_two_observed_upper_points_are_bound_without_freezing_soft_priors(self):
        pins = self.native_contacts(431, (0, -1))
        heights, note = solve_profiles(self.plan, fixed_contacts=pins)
        self.assertIsNotNone(heights, note)
        self.assertLess(
            max(
                audit_fixed_contacts(self.plan, heights, pins)[
                    "height_errors_m"
                ].values()
            ),
            1e-7,
        )
        _body, data, _geometry = build(self.plan, heights)
        audit_profiles(self.plan, heights, data)

    def test_real_lower_point_is_infeasible_if_caller_declares_it_fixed(self):
        heights, note = solve_profiles(
            self.plan, fixed_contacts=self.native_contacts(432, (0,))
        )
        self.assertIsNone(heights)
        self.assertFalse(note["valid"])
        self.assertEqual(note["max_prior_change_budget_m"], 2)
        self.assertIn("infeasible", note["status"].lower())

    def test_whole_synthetic_outer_profile_is_fixed_after_a_height_change(self):
        _body, data, _geometry = build(self.plan, self.heights)
        product = shared_products(self.plan, self.heights, data)["external_contacts"][0]
        xyz = np.array(product["profile_xyz_m"])
        xyz[:, 2] += 0.05
        pin = contact("synthetic_shifted_profile", product["source_id"], xyz.tolist())
        before = copy.deepcopy(self.plan["source_metadata"])
        heights, note = solve_profiles(self.plan, fixed_contacts=[pin])
        self.assertIsNotNone(heights, note)
        self.assertLess(
            audit_fixed_contacts(self.plan, heights, [pin])["height_errors_m"][
                pin["contact_id"]
            ],
            1e-7,
        )
        self.assertEqual(before, self.plan["source_metadata"])
        _body, after, _geometry = build(self.plan, heights)
        audit_profiles(self.plan, heights, after)

    def test_all_four_prepared_outer_profiles_can_be_rebound_as_explicit_controls(self):
        _body, data, _geometry = build(self.plan, self.heights)
        products = shared_products(self.plan, self.heights, data)
        pins = [
            contact(f"roundtrip:{i}", p["source_id"], p["profile_xyz_m"])
            for i, p in enumerate(products["external_contacts"])
        ]
        heights, note = solve_profiles(self.plan, fixed_contacts=pins)
        self.assertIsNotNone(heights, note)
        errors = audit_fixed_contacts(self.plan, heights, pins)["height_errors_m"]
        self.assertEqual(len(errors), 4)
        self.assertLess(max(errors.values()), 1e-7)
        _body, after, _geometry = build(self.plan, heights)
        audit_profiles(self.plan, heights, after)


if __name__ == "__main__":
    unittest.main()
