"""Prepared reference reuse, exact draw products and view-independent physical access."""

import copy
import unittest
from types import SimpleNamespace
from unittest.mock import patch

import manifold3d as manifold
import numpy as np
from demand import View
from facade_parts import append
from prepared_hall import HallInstance, prepare_asset, selection
from recipe_hall_operators import inputs_for
from recipe_hall_operators import prepare as old_recipe
from recipe_rooflight_operators import prepare
from recipe_rooflight_products import assemble as full_assembly
from rooflight_lamina import assemble
from rooflight_lamina import selection as old_selection
from structural_shell_geometry import native
from verify_industrial_halls import (
    hall_route,
    loading_access,
    personnel_access,
    sources,
    view_for,
)
from verify_recipe_halls import mesh_equal


def run_for(row, neighbors):
    return prepare().evaluate(inputs_for(row, neighbors))


def view_for_factor(row, factor):
    v = view_for(row)
    return View(
        v.target + factor * (v.eye - v.target), v.target, width=1920, height=1080
    )


class PreparedHall(unittest.TestCase):
    def test_render_matches_existing_real_assembly_for_every_state_and_distance(self):
        for row, neighbors in sources()[:3]:
            run = run_for(row, neighbors)
            actor = HallInstance(prepare_asset(run))
            for factor in (1, 8):
                view = view_for_factor(row, factor)
                for opened, angle in ((0, 0), (0.5, 90), (1, 90)):
                    expected, _, _ = assemble(
                        run, view, opened=opened, staff_angle=angle
                    )
                    draw = actor.update(view, opened=opened, staff_angle=angle)
                    mesh_equal(append(draw.body, draw.render_parts), expected)

    def test_physical_ribs_and_all_existing_access_remain_independent_of_view(self):
        for row, neighbors in sources()[:3]:
            run = run_for(row, neighbors)
            asset = prepare_asset(run)
            actor = HallInstance(asset)
            v = view_for_factor(row, 1)
            precise = View(
                v.eye, v.target, width=1920, height=1080, detail_error_px=1e-12
            )
            for opened, angle in ((0, 0), (0.5, 90), (1, 90)):
                draw = actor.update(
                    view_for_factor(row, 8), opened=opened, staff_angle=angle
                )
                expected, _, _ = full_assembly(
                    run, precise, opened=opened, staff_angle=angle
                )
                mesh_equal(append(draw.body, draw.physical_parts), expected)
                shell = run.outputs["shell"].value
                collision = manifold.Manifold.batch_boolean(
                    [
                        native(draw.body, shell.shell.vertex_roots),
                        *[native(c) for c in draw.physical_parts],
                    ],
                    manifold.OpType.Add,
                )
                self.assertEqual(collision.status(), manifold.Error.NoError)
                loading_access(asset.plan, collision, opened)
                personnel_access(asset.plan, collision, angle)
                hall_route(asset.plan, collision)

    def test_precomputed_bounds_match_existing_policy_without_expanding_fields(self):
        for row, neighbors in sources()[:3]:
            asset = prepare_asset(run_for(row, neighbors))
            for factor in (1, 4, 8):
                view = view_for_factor(row, factor)
                mask, errors = selection(asset, view)
                expected = old_selection(asset.run.outputs["rooflights"].value, view)
                self.assertEqual(mask.tolist(), [p["lamina"] for p in expected])
                for a, b in zip(errors, expected, strict=True):
                    if b["geometry_bound_px"] is None:
                        self.assertFalse(np.isfinite(a))
                    else:
                        self.assertAlmostEqual(a, b["geometry_bound_px"], places=12)

    def test_camera_changes_reuse_all_geometry_and_physical_references(self):
        row, neighbors = sources()[2]
        actor = HallInstance(prepare_asset(run_for(row, neighbors)))
        with (
            patch(
                "prepared_hall.personnel_leaf",
                side_effect=AssertionError("unchanged door"),
            ),
            patch(
                "prepared_hall.roller_leaf",
                side_effect=AssertionError("unchanged gate"),
            ),
            patch(
                "prepared_hall.windows", side_effect=AssertionError("unchanged windows")
            ),
            patch(
                "prepared_hall.sheets", side_effect=AssertionError("unchanged sheets")
            ),
            patch(
                "prepared_hall.closed",
                side_effect=AssertionError("unchanged full parts"),
            ),
        ):
            first = actor.update(view_for_factor(row, 1))
            self.assertIs(actor.update(view_for_factor(row, 1)), first)
            for factor in (4, 8, 1):
                draw = actor.update(view_for_factor(row, factor))
                self.assertIs(draw.body, first.body)
                self.assertIs(draw.physical_parts, first.physical_parts)
                self.assertEqual(actor.generated_components, 0)
        self.assertTrue(
            all(not a.flags.writeable for p in actor.asset.fixed for a in p.values())
        )

    def test_state_updates_only_generate_moving_leaves(self):
        row, neighbors = sources()[2]
        actor = HallInstance(prepare_asset(run_for(row, neighbors)))
        initial = actor.update(view_for_factor(row, 1))
        with patch(
            "prepared_hall.windows", side_effect=AssertionError("unneeded static work")
        ):
            next_draw = actor.update(
                view_for_factor(row, 1), opened=0.5, staff_angle=90
            )
        self.assertEqual(actor.generated_components, 3)
        self.assertIs(initial.body, next_draw.body)
        self.assertIs(initial.render_parts[0], next_draw.render_parts[0])
        self.assertIs(
            actor.update(view_for_factor(row, 1), opened=0.5, staff_angle=90), next_draw
        )
        self.assertEqual(actor.generated_components, 0)

    def test_warm_state_aliases_and_invalid_updates_preserve_previous_instance(self):
        row, neighbors = sources()[1]
        actor = HallInstance(prepare_asset(run_for(row, neighbors)))
        view = view_for_factor(row, 1)
        original = actor.update(view)
        for arguments in (
            {"opened": True},
            {"staff_angle": False},
            {"opened": float("nan")},
            {"opened": 2},
            {"staff_angle": 91},
        ):
            with self.assertRaises(ValueError):
                actor.update(view, **arguments)
            self.assertIs(actor.draw, original)
            self.assertEqual((actor.opened, actor.angle), (0, 0))

    def test_new_source_content_changes_asset_identity_without_changing_original(self):
        row, neighbors = sources()[2]
        old = prepare_asset(run_for(row, neighbors))
        body = {k: v.copy() for k, v in old.body.items()}
        changed = copy.deepcopy(row)
        changed["properties"]["render_height"] = 6
        new = prepare_asset(run_for(changed, neighbors))
        self.assertEqual(old.plan["room_id"], new.plan["room_id"])
        self.assertNotEqual(old.run.instance_fingerprint, new.run.instance_fingerprint)
        for k, v in old.body.items():
            np.testing.assert_array_equal(v, body[k])
        with self.assertRaises(TypeError):
            old.fixed[0]["materials"] = np.zeros(1)

    def test_wrong_recipe_and_mixed_actual_source_ownership_reject(self):
        a, an = sources()[0]
        b, bn = sources()[1]
        with self.assertRaises(ValueError):
            prepare_asset(old_recipe().evaluate(inputs_for(a, an)))
        first, second = run_for(a, an), run_for(b, bn)
        changed = dict(first.outputs, programme=second.outputs["programme"])
        with self.assertRaisesRegex(ValueError, "ownership"):
            prepare_asset(SimpleNamespace(outputs=changed))


if __name__ == "__main__":
    unittest.main()
