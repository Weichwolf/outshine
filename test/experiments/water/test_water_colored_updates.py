"""Finite dependency cones, independent completion and original physical checks."""

import dataclasses
import unittest
from types import SimpleNamespace

import numpy as np
from compare_water_colored_updates import make_recipe
from compare_water_grid_height import DEFAULT
from water_colored_reference import ReferenceUpdates
from water_colored_updates import (
    ColoredUpdates,
    dependencies,
    incident_faces,
    preflight,
)
from water_nodal_height import audit_face
from water_owner_inputs import LAYERS, LayerReceipt

EPOCH = ("provider", "revision")


def receipts():
    return {
        ((6, x, y), layer): LayerReceipt(
            (6, x, y), layer, EPOCH, f"{layer}:{x}:{y}", True
        )
        for x in range(14, 27)
        for y in range(14, 27)
        for layer in LAYERS
    }


def synthetic():
    parent = lambda k: np.array([100 * k[1], 100 * k[2], 0.0], dtype=float)
    target = lambda k: SimpleNamespace(
        state="ready", world=parent(k) + [0, 0, 3.0 if k[1] % 2 else -2.0]
    )

    def check(keys, points):
        slope = np.linalg.solve(
            points[1:, :2] - points[0, :2], points[1:, 2] - points[0, 2]
        )
        return "admitted" if np.max(np.abs(slope)) <= 0.01 else "physical_rejection"

    return ColoredUpdates(target, parent, check), check


class FiniteUpdates(unittest.TestCase):
    def test_colours_have_disjoint_faces_and_three_hop_dependency_bound(self):
        items = receipts()
        for x in range(36, 42):
            for y in range(36, 42):
                key = (6, x, y)
                self.assertEqual(len(incident_faces(key)), 6)
                for face in incident_faces(key):
                    self.assertEqual({(p[1] + p[2]) % 3 for p in face}, {0, 1, 2})
                selected = dependencies(key)
                self.assertLessEqual(len(selected), 37)
                self.assertTrue(
                    all(max(abs(p[i] - key[i]) for i in (1, 2)) <= 3 for p in selected)
                )
                plan = preflight(key, items, EPOCH, "parent-v1")
                self.assertEqual(plan["state"], "ready")
                self.assertLessEqual(len(plan["required"]), 98)

    def test_reverse_order_preserves_all_completed_faces(self):
        first, check = synthetic()
        second, _ = synthetic()
        face = incident_faces((6, 40, 40))[0]
        one = {p: first.node(p) for p in face}
        two = {p: second.node(p) for p in reversed(face)}
        for key in face:
            self.assertEqual(one[key].state, "ready")
            np.testing.assert_array_equal(one[key].world, two[key].world)
        self.assertEqual(
            check(face, np.array([one[p].world for p in face])), "admitted"
        )
        self.assertTrue(any(n.denominator != 1 for n in one.values()))

    def test_missing_receipts_stop_before_any_profile_or_physical_work(self):
        recipe = ReferenceUpdates(
            {}, EPOCH, {}, {}, {}, parent_level_m=450.0, parent_version="parent-v1"
        )
        result = recipe.resolve((6, 40, 40))
        self.assertEqual(result.state, "pending_sources")
        self.assertIsNone(result.world)
        self.assertFalse(recipe.targets)
        self.assertEqual(recipe.field.work["states"], 0)
        self.assertEqual(recipe.work["physical_audits"], 0)

    def test_parent_source_changes_and_projection_contacts_are_explicit(self):
        key = (6, 40, 40)
        items = receipts()
        first = preflight(key, items, EPOCH, "parent-v1")
        self.assertNotEqual(
            first["version"], preflight(key, items, EPOCH, "parent-v2")["version"]
        )
        required = first["required"][0]
        old = items[required]
        items[required] = dataclasses.replace(old, digest="changed-source")
        self.assertNotEqual(
            first["version"], preflight(key, items, EPOCH, "parent-v1")["version"]
        )
        items[required] = old
        items[((6, 14, 14), LAYERS[0])] = dataclasses.replace(
            items[((6, 14, 14), LAYERS[0])], digest="outside"
        )
        self.assertEqual(
            first["version"], preflight(key, items, EPOCH, "parent-v1")["version"]
        )
        items[required] = dataclasses.replace(items[required], complete=False)
        self.assertEqual(
            preflight(key, items, EPOCH, "parent-v1")["state"], "pending_sources"
        )
        edge = preflight((6, 0, 40), items, EPOCH, "parent-v1")
        self.assertEqual(edge["state"], "needs_projection_contacts")
        self.assertEqual(edge["receipt_lookups"], 0)

    def test_budget_and_invalid_parent_never_publish_a_fallback(self):
        field, _ = synthetic()
        field.state_limit = 1
        self.assertIsNone(field.node((6, 40, 40)).world)
        field, _ = synthetic()
        field.admissible = lambda *args: "physical_rejection"
        result = field.node((6, 39, 39))
        self.assertEqual(result.state, "parent_refused")
        self.assertIsNone(result.world)


class OriginalWaterUpdates(unittest.TestCase):
    def test_both_original_rejections_become_admitted_without_a_height_qp(self):
        recipe, _ = make_recipe(DEFAULT, 450.0)
        faces = (
            ((14, 17257, 11493), (14, 17257, 11494), (14, 17258, 11494)),
            ((14, 17257, 11494), (14, 17258, 11495), (14, 17258, 11494)),
        )
        for face in faces:
            direct = {p: recipe.target(p) for p in face}
            self.assertEqual(
                audit_face(face, direct, recipe.data)[0]["state"], "physical_rejection"
            )
            result = {p: recipe.resolve(p) for p in face}
            self.assertTrue(all(n.state == "ready" for n in result.values()))
            updated = {
                p: dataclasses.replace(direct[p], world=result[p].world) for p in face
            }
            report, _ = audit_face(face, updated, recipe.data)
            self.assertEqual(report["state"], "admitted")
            self.assertEqual(report["sampled"]["physical_grade_violation"], 0)
            for p in face:
                self.assertLessEqual(
                    np.linalg.norm(updated[p].world - direct[p].world),
                    np.linalg.norm(recipe.parent(p) - direct[p].world) + 1e-9,
                )
        incompatible = ReferenceUpdates(
            recipe.receipts,
            recipe.epoch,
            recipe.snapshots,
            recipe.data,
            recipe.order,
            parent_level_m=0.0,
            parent_version=recipe.parent_recipe_version,
        )
        self.assertNotEqual(recipe.parent_version, incompatible.parent_version)
        rejected = incompatible.resolve((14, 17258, 11494))
        self.assertEqual(rejected.state, "parent_refused")
        self.assertIsNone(rejected.world)


if __name__ == "__main__":
    unittest.main()
