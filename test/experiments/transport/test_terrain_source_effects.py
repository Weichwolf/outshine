"""Explicit terrain intent stays on actual instances, indexed faces and undersides."""

import copy
import json
import unittest

import numpy as np
from compare_mvt_terrain_effects import controlled_roles, prepare
from mvt_independent_flight_body import construct
from mvt_t_profile_products import shared_products
from terrain_source_effects import qualify_effects


class TerrainSourceEffectTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = {
            name: construct(name, cut_stage="canonical")
            for name in ("stacked", "crossed")
        }

    def test_roles_follow_instances_and_actual_bottom_faces(self):
        for name, case in self.cases.items():
            with self.subTest(name=name):
                plan, data = case["plan"], case["data"]
                result = qualify_effects(plan, data, controlled_roles(case))
                upper = set(plan["controlled_derivation"]["upper_source_mapping"])
                upper_mask = sum(
                    1 << i
                    for i, r in enumerate(plan["rows"])
                    if r["source_id"] in upper
                )
                selected = result["body_cap_face_ids"]
                np.testing.assert_array_equal(
                    selected, np.arange(len(data["bottom_faces"]))
                )
                np.testing.assert_array_equal(
                    result["effects"] == 2,
                    (data["face_sources"][selected] & upper_mask) != 0,
                )
                np.testing.assert_array_equal(
                    result["height_offsets_m"], np.where(result["effects"] == 2, -2, 0)
                )
                self.assertEqual(result["surface_array"], "body_vertices")
                self.assertEqual(result["face_array"], "bottom_faces")
                self.assertEqual(
                    result["height_policy"]["OverheadClearance"], "ceiling_only"
                )
                underside = data[result["surface_array"]][
                    data[result["face_array"]][selected]
                ]
                top = data["body_vertices"][data["top_faces"][selected]]
                self.assertTrue(
                    np.all(underside[:, :, 2].mean(axis=1) < top[:, :, 2].mean(axis=1))
                )
                self.assertEqual(result["work"]["point_bytes_copied"], 0)
                self.assertLess(
                    result["work"]["resolved_owner_bits"],
                    result["work"]["owner_incidences"],
                )

    def test_original_three_sources_gain_explicit_underside_contacts(self):
        case = prepare("original")
        result = shared_products(
            case["plan"],
            case["heights"],
            case["data"],
            terrain_declarations=controlled_roles(case),
        )
        effects = result.pop("terrain_effects")
        self.assertEqual(result, case["products"])
        self.assertEqual(len(effects["sources"]), 3)
        self.assertEqual(
            len(effects["body_cap_face_ids"]), len(case["data"]["bottom_faces"])
        )
        np.testing.assert_array_equal(effects["effects"], 1)
        np.testing.assert_array_equal(effects["height_offsets_m"], 0)
        self.assertEqual(
            case["original_report"]["orientation"]["invalid_float32_faces"], 0
        )

    def test_captured_world_parts_keep_their_own_body_undersides_and_parent_facts(self):
        case = prepare("world")
        declarations = controlled_roles(case)
        before_capture = json.dumps(case["capture"], sort_keys=True)
        before_arrays = {k: a.tobytes() for k, a in case["data"].items()}
        result = qualify_effects(case["plan"], case["data"], declarations)
        self.assertEqual(len(result["sources"]), 3)
        self.assertEqual(len({r["parent_source_id"] for r in case["plan"]["rows"]}), 1)
        np.testing.assert_array_equal(
            result["body_cap_face_ids"], np.arange(len(case["data"]["bottom_faces"]))
        )
        np.testing.assert_array_equal(result["effects"], 1)
        np.testing.assert_array_equal(result["contact_keys"], 1)
        np.testing.assert_array_equal(result["height_offsets_m"], 0)
        self.assertEqual(result["face_array"], "bottom_faces")
        collapsed = {
            r["parent_source_id"]: declarations[r["source_id"]]
            for r in case["plan"]["rows"]
        }
        with self.assertRaisesRegex(ValueError, "terrain role instance mismatch"):
            qualify_effects(case["plan"], case["data"], collapsed)
        self.assertEqual(json.dumps(case["capture"], sort_keys=True), before_capture)
        for key, words in before_arrays.items():
            self.assertEqual(case["data"][key].tobytes(), words)

    def test_ceiling_targets_never_require_raising_lower_terrain(self):
        for case in self.cases.values():
            data = case["data"]
            result = qualify_effects(case["plan"], data, controlled_roles(case))
            ids = result["body_cap_face_ids"]
            target = (
                data["body_vertices"][data["bottom_faces"][ids], 2].mean(axis=1)
                + result["height_offsets_m"]
            )
            is_ceiling = result["effects"] == 2
            for before in (target - 3, target + 3):
                # Independently consume the declared ceiling policy at actual face centroids.
                after = np.minimum(before[is_ceiling], target[is_ceiling])
                self.assertTrue(np.all(after <= before[is_ceiling]))
                if np.all(before < target):
                    np.testing.assert_array_equal(after, before[is_ceiling])

    def test_original_provider_keys_cannot_collapse_derived_instances(self):
        for case in self.cases.values():
            mapping = case["plan"]["controlled_derivation"]["upper_source_mapping"]
            collapsed = {
                mapping.get(k, k): v for k, v in controlled_roles(case).items()
            }
            with self.assertRaisesRegex(ValueError, "missing=.*derived-upper"):
                qualify_effects(case["plan"], case["data"], collapsed)

    def test_unknown_roles_are_not_explicit_no_earthwork(self):
        case = self.cases["stacked"]
        for value in (
            {},
            {"effect": "Unspecified"},
            {"effect": "unknown"},
            {"effect": True},
        ):
            declarations = controlled_roles(case)
            declarations[next(iter(declarations))] = value
            with self.assertRaisesRegex(ValueError, "qualified terrain effect"):
                qualify_effects(case["plan"], case["data"], declarations)
        declarations = {
            k: dict(v, effect="NoEarthwork", recipe="Controlled explicit exclusion.")
            for k, v in controlled_roles(case).items()
        }
        result = qualify_effects(case["plan"], case["data"], declarations)
        self.assertEqual(len(result["body_cap_face_ids"]), 0)
        self.assertEqual(result["work"]["selected_array_bytes"], 0)
        self.assertEqual(
            result["work"]["no_earthwork_faces"], len(case["data"]["bottom_faces"])
        )

    def test_ground_priority_and_contact_conflicts_use_actual_shared_faces(self):
        case = self.cases["stacked"]
        ids = [r["source_id"] for r in case["plan"]["rows"]]
        shared = (case["data"]["face_sources"] & 3) == 3
        self.assertTrue(shared.any())
        declarations = controlled_roles(case)
        declarations[ids[0]]["relief_m"] = 0.5
        declarations[ids[1]].update(
            effect="OverheadClearance", clearance_m=4, relief_m=9
        )
        result = qualify_effects(case["plan"], case["data"], declarations)
        selected = shared[result["body_cap_face_ids"]]
        np.testing.assert_array_equal(result["effects"][selected], 1)
        np.testing.assert_array_equal(result["height_offsets_m"][selected], 0)
        np.testing.assert_array_equal(result["relief_m"][selected], 0.5)
        declarations = controlled_roles(case)
        declarations[ids[1]]["contact_key"] = 2
        with self.assertRaisesRegex(
            ValueError, "conflicting ground contact keys.*sources"
        ):
            qualify_effects(case["plan"], case["data"], declarations)

    def test_source_and_declaration_order_do_not_change_face_effects(self):
        case = self.cases["crossed"]
        declarations = controlled_roles(case)
        reference = qualify_effects(case["plan"], case["data"], declarations)
        plan, data = dict(case["plan"]), dict(case["data"])
        n = len(plan["rows"])
        plan["rows"] = plan["rows"][::-1]
        masks = data["face_sources"]
        data["face_sources"] = sum(((masks >> i) & 1) << (n - 1 - i) for i in range(n))
        result = qualify_effects(plan, data, dict(reversed(list(declarations.items()))))
        for key in (
            "body_cap_face_ids",
            "effects",
            "contact_keys",
            "height_offsets_m",
            "relief_m",
        ):
            np.testing.assert_array_equal(result[key], reference[key])

    def test_shared_clearance_and_ground_use_their_own_maximum_relief(self):
        case = self.cases["stacked"]
        ids = [r["source_id"] for r in case["plan"]["rows"]]
        shared = case["data"]["face_sources"] == 3
        self.assertTrue(shared.any())
        declarations = controlled_roles(case)
        declarations[ids[0]].update(
            effect="OverheadClearance", clearance_m=1, relief_m=9
        )
        declarations[ids[1]].update(
            effect="OverheadClearance", clearance_m=4, relief_m=2
        )
        result = qualify_effects(case["plan"], case["data"], declarations)
        selected = shared[result["body_cap_face_ids"]]
        np.testing.assert_array_equal(result["effects"][selected], 2)
        np.testing.assert_array_equal(result["contact_keys"][selected], 0)
        np.testing.assert_array_equal(result["height_offsets_m"][selected], -4)
        np.testing.assert_array_equal(result["relief_m"][selected], 9)
        for key, relief in ((ids[0], 0.5), (ids[1], 0.75)):
            declarations[key].update(
                effect="GroundContact", contact_key=1, relief_m=relief
            )
        result = qualify_effects(case["plan"], case["data"], declarations)
        np.testing.assert_array_equal(result["relief_m"][selected], 0.75)

    def test_mask_factoring_matches_direct_per_face_resolution(self):
        for case in self.cases.values():
            declarations = controlled_roles(case)
            factored = qualify_effects(case["plan"], case["data"], declarations)
            direct = qualify_effects(
                case["plan"], case["data"], declarations, reuse_masks=False
            )
            for key in (
                "body_cap_face_ids",
                "effects",
                "contact_keys",
                "height_offsets_m",
                "relief_m",
            ):
                np.testing.assert_array_equal(factored[key], direct[key])
            self.assertEqual(factored["sources"], direct["sources"])
            self.assertEqual(
                direct["work"]["resolved_owner_bits"],
                direct["work"]["owner_incidences"],
            )
            self.assertLess(
                factored["work"]["resolved_owner_bits"],
                direct["work"]["resolved_owner_bits"],
            )

    def test_invalid_values_are_rejected_before_any_product(self):
        case = self.cases["stacked"]
        for values in (
            {"contact_key": 0},
            {"contact_key": -1},
            {"contact_key": 2**64},
            {"contact_key": True},
            {"clearance_m": -1},
            {"relief_m": float("nan")},
            {"clearance_m": None},
            {"clearance_m": 2**1024},
            {"relief_m": "0"},
            {"recipe": ""},
        ):
            declarations = controlled_roles(case)
            declarations[next(iter(declarations))].update(values)
            with self.subTest(values=values), self.assertRaises(ValueError):
                qualify_effects(case["plan"], case["data"], declarations)

    def test_invalid_face_ownership_and_cap_indices_are_locally_rejected(self):
        case = self.cases["stacked"]
        declarations = controlled_roles(case)
        for mask in (0, -1, 1 << len(case["plan"]["rows"])):
            data = dict(case["data"])
            data["face_sources"] = np.full(len(data["bottom_faces"]), mask)
            with self.assertRaisesRegex(ValueError, "invalid source-instance mask"):
                qualify_effects(case["plan"], data, declarations)
        data = dict(case["data"])
        data["bottom_faces"] = data["bottom_faces"].copy()
        data["bottom_faces"][0, 0] = len(data["body_vertices"])
        with self.assertRaisesRegex(ValueError, "indexed body underside"):
            qualify_effects(case["plan"], data, declarations)

    def test_budget_and_midwork_cancellation_preserve_all_inputs(self):
        case = self.cases["stacked"]
        declarations = controlled_roles(case)
        before_declarations = copy.deepcopy(declarations)
        before_arrays = {k: a.tobytes() for k, a in case["data"].items()}
        before_metadata = json.dumps(case["plan"]["source_metadata"], sort_keys=True)
        calls = iter([False, False, True])
        for limits in ({"max_faces": 0}, {"cancelled": lambda: next(calls)}):
            with self.assertRaises(ValueError):
                qualify_effects(case["plan"], case["data"], declarations, **limits)
        self.assertEqual(declarations, before_declarations)
        self.assertEqual(
            before_metadata, json.dumps(case["plan"]["source_metadata"], sort_keys=True)
        )
        for key, words in before_arrays.items():
            self.assertEqual(case["data"][key].tobytes(), words)

    def test_default_shared_products_keep_unqualified_reservations(self):
        for case in self.cases.values():
            result = shared_products(case["plan"], case["heights"], case["data"])
            self.assertEqual(result, case["products"])
            self.assertNotIn("terrain_effects", result)

    def test_optional_product_is_json_serializable_and_preserves_other_consumers(self):
        for case in self.cases.values():
            result = shared_products(
                case["plan"],
                case["heights"],
                case["data"],
                terrain_declarations=controlled_roles(case),
            )
            effects = result.pop("terrain_effects")
            self.assertEqual(result, case["products"])
            self.assertEqual(effects["surface_array"], "body_vertices")
            self.assertEqual(effects["face_array"], "bottom_faces")
            self.assertEqual(len(effects["sources"]), 6)
            json.dumps(effects)

    def test_highest_source_bit_and_contact_key_keep_unsigned_identity(self):
        plan = {"rows": [{"source_id": i} for i in range(64)]}
        points = np.array([[0, 0, 0], [2, 0, 0], [0, 2, 0]], dtype=float)
        data = {
            "body_vertices": points,
            "bottom_faces": np.array([[0, 2, 1]]),
            "face_sources": np.array([1 << 63], dtype=np.uint64),
        }
        declarations = {
            i: {"effect": "NoEarthwork", "recipe": "Controlled exclusion."}
            for i in range(64)
        }
        declarations[63] = {
            "effect": "GroundContact",
            "contact_key": 2**63 + 7,
            "recipe": "Controlled highest-bit support.",
        }
        result = qualify_effects(plan, data, declarations)
        np.testing.assert_array_equal(result["effects"], [1])
        self.assertEqual(int(result["contact_keys"][0]), 2**63 + 7)
        self.assertEqual(result["sources"][63]["contact_key"], 2**63 + 7)


if __name__ == "__main__":
    unittest.main()
