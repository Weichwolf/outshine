"""Complete canonical plant IDs, independent anchors and immutable demand products."""

import json
import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import AxisLevel, Blade, PlantPlan, Shoot
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.shoot_plan import normal_frame


def plant(seed=17):
    return PlantPlan(
        seed,
        (
            AxisLevel(1, 2, 0.04, 0.03, tip_scale=1, bend=0, bend_variation=0),
            AxisLevel(
                2,
                0.5,
                0.01,
                0.005,
                bend=0.2,
                leaf_internode=0.1,
                first=0.25,
                last=0.75,
                attachment_jitter=0,
            ),
        ),
        (Blade(), replace(Blade(), width=0.05)),
    )


class PlantTests(unittest.TestCase):
    def test_complete_tables_have_parent_links_owners_and_cached_immutable_rows(self):
        plan = plant()
        summary = plan.summary()
        self.assertIsNone(plan.tables(0, 100))
        self.assertFalse(plan._axes or plan._leaves)
        tables = plan.tables(summary["axes"], summary["leaves"])
        self.assertIs(tables, plan.tables(100, 100))
        np.testing.assert_array_equal(tables["axis_parent_ids"], [-1, 0, 0])
        np.testing.assert_array_equal(tables["axis_paths"][:, 0], [-1, 0, 1])
        for axis in plan.expand(100, 100)["axes"]:
            np.testing.assert_array_equal(tables["axis_origins"][axis.id], axis.origin)
            group = plan.leaves(axis.path, 100)
            if group is not None:
                mask = tables["leaf_axis_ids"] == axis.id
                for name in group:
                    np.testing.assert_array_equal(
                        tables["leaf_" + name][mask], group[name]
                    )
        for values in tables.values():
            with self.assertRaises(ValueError):
                values.setflags(write=True)
        empty = PlantPlan(0, (plan.levels[0],), plan.blades).tables(1, 0)
        self.assertEqual(empty["leaf_frames"].shape, (0, 3, 3))
        self.assertEqual(empty["leaf_ids"].dtype, np.uint64)

    def test_complete_ids_variants_and_independent_trunk_anchors(self):
        plan = plant()
        summary = plan.summary()
        self.assertFalse(plan._axes or plan._leaves)
        detail = plan.expand(summary["axes"], summary["leaves"])
        self.assertEqual(len(detail["axes"]), 3)
        np.testing.assert_array_equal([axis.id for axis in detail["axes"]], [0, 1, 2])
        ids = np.concatenate([g["ids"] for g in detail["groups"]])
        np.testing.assert_array_equal(
            ids, np.arange(summary["leaves"], dtype=np.uint64)
        )
        variants = np.concatenate([g["variants"] for g in detail["groups"]])
        np.testing.assert_array_equal(
            np.bincount(variants, minlength=2), summary["variant_counts"]
        )
        for slot, z in enumerate((0.5, 1.5)):
            axis = plan.axis((slot,))
            np.testing.assert_allclose(axis.origin, [0, 0, z], atol=1e-15)
            np.testing.assert_allclose(axis.frame.T @ axis.frame, np.eye(3), atol=1e-15)
            self.assertAlmostEqual(np.linalg.det(axis.frame), 1)
        for group in detail["groups"]:
            roots = group["origins"] - plan.blades[0].petiole * group["frames"][:, :, 1]
            np.testing.assert_allclose(roots, group["nodes"], atol=5e-16)

    def test_recipe_and_reversed_partial_requests_preserve_all_records(self):
        plan = plant(2**64 - 1)
        restored = PlantPlan.from_recipe(json.loads(json.dumps(plan.recipe())))
        self.assertEqual(plan.identity, restored.identity)
        summary = plan.summary()
        full = plan.expand(summary["axes"], summary["leaves"])
        for path in reversed(tuple(restored.paths())):
            left, right = plan.axis(path), restored.axis(path)
            self.assertEqual(left.id, right.id)
            np.testing.assert_array_equal(left.origin, right.origin)
            np.testing.assert_array_equal(left.frame, right.frame)
            a, b = (
                plan.leaves(path, summary["leaves"]),
                restored.leaves(path, summary["leaves"]),
            )
            if a is not None:
                for name in a:
                    np.testing.assert_array_equal(a[name], b[name])
        self.assertEqual(len(full["axes"]), restored.summary()["axes"])

    def test_shared_states_and_shoot_tables_cannot_be_poisoned(self):
        plan = plant()
        state = plan.axis((0,))
        group = plan.leaves((0,), 100)
        shoot = Shoot().detail(100)
        for array in [state.origin, state.frame, *group.values(), *shoot.values()]:
            with self.assertRaises(ValueError):
                array.setflags(write=True)
        for mapping in (group, shoot):
            with self.assertRaises(TypeError):
                mapping["ids"] = np.zeros(1)
        origin, frame = state.origin.copy(), state.frame.copy()
        owned = replace(state, origin=origin, frame=frame)
        origin[:] = 123
        frame[:] = 0
        np.testing.assert_array_equal(owned.origin, state.origin)
        np.testing.assert_array_equal(owned.frame, state.frame)
        with self.assertRaises(ValueError):
            replace(state, frame=np.diag([1, 1, -1]))

    def test_insufficient_budget_does_no_placement_work(self):
        plan = plant()
        summary = plan.summary()
        with patch.object(PlantPlan, "axis", side_effect=AssertionError("placement")):
            self.assertIsNone(plan.expand(summary["axes"] - 1, summary["leaves"]))
            self.assertIsNone(plan.expand(summary["axes"], summary["leaves"] - 1))
            self.assertIsNone(plan.leaves((0,), 0))
        self.assertFalse(plan._axes or plan._leaves)
        for budget in (True, -1, 100001, 1.5):
            with self.assertRaises(ValueError):
                plan.expand(budget, 100)
            with self.assertRaises(ValueError):
                Shoot().detail(budget)
        large = replace(Shoot(), internode=1e-6)
        self.assertIsNone(large.detail(100000))
        self.assertNotIn("_placements", large.__dict__)

    def test_frames_and_tubes_reject_bad_inputs_and_keep_closed_volume(self):
        for direction, hint in (([0, 0, 0], [0, 0, 1]), ([1, np.nan, 0], [0, 0, 1])):
            with self.assertRaises(ValueError):
                normal_frame(direction, hint)
        np.testing.assert_allclose(
            normal_frame([0, 0, 1], [0, 0, 1]).T @ normal_frame([0, 0, 1], [0, 0, 1]),
            np.eye(3),
        )
        mesh = axis_mesh(2, 0, 0.1, 0.1, 8, 16)
        p = mesh["vertices"][mesh["faces"]]
        volume = np.einsum("ij,ij->i", p[:, 0], np.cross(p[:, 1], p[:, 2])).sum() / 6
        expected = 2 * 16 / 2 * np.sin(2 * np.pi / 16) * 0.1**2
        self.assertAlmostEqual(volume, expected, places=14)
        bark = axis_mesh(2, 0, 0.1, 0.1, 8, 16, bark=True)
        for name, values in mesh.items():
            np.testing.assert_array_equal(values, bark[name])
        self.assertEqual(bark["bark_coordinates"].shape, mesh["vertices"].shape)
        np.testing.assert_allclose(
            np.linalg.norm(bark["bark_coordinates"][:-2, :2], axis=1), 0.1, atol=1e-15
        )
        np.testing.assert_array_equal(bark["bark_radius"], 0.1)
        with self.assertRaises(ValueError):
            axis_mesh(2, 0, 0.1, 0.1, True, 16)
        with self.assertRaises(ValueError):
            PlantPlan.from_recipe(plant().recipe() | {"version": "unknown"})


if __name__ == "__main__":
    unittest.main()
