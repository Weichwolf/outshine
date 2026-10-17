"""Versioned real glazed hall products retain ownership, function and early demand."""

import copy
import json
import unittest
from unittest.mock import patch

import manifold3d as manifold
import numpy as np
from industrial_hall_plan import metadata
from outshine_ref.buildings.rooflight_panels import closed
from provider_contract import attributes
from recipe_graph_data import Budget, canonical
from recipe_hall_operators import inputs_for
from recipe_hall_operators import prepare as previous
from recipe_rooflight_operators import ROOT, panel_representation, prepare
from recipe_rooflight_products import assemble
from rooflight_surface_probe import prepared
from structural_shell_geometry import native
from tile_facades import FacadeDetails
from tile_industrial_halls import HallDetails
from verify_industrial_halls import (
    hall_route,
    loading_access,
    personnel_access,
    sources,
    view_for,
)
from verify_industrial_rooflights import light_paths, roof_coverage
from verify_recipe_halls import mesh_equal


class RecipeRooflights(unittest.TestCase):
    def test_normal_tile_builder_v2_matches_graph_without_any_prototype_patch(self):
        for row, neighbors in sources()[:3]:
            view = view_for(row)
            run = prepare().evaluate(inputs_for(row, neighbors))
            owner = FacadeDetails(
                [row, *neighbors],
                view,
                door_states={row["id"] + ":floor-0:entrance": 90},
            )
            owner.halls = HallDetails(
                owner.entrances,
                {row["id"] + ":floor-0:loading-0": 1},
                version="2",
            )
            mesh, info = owner(row, attributes(row["properties"]), 1)
            self.assertEqual(info["hall_recipe"], "buildings.industrial-hall/2")
            expected, _, _ = assemble(run, view, opened=1, staff_angle=90)
            mesh_equal(mesh, expected)
            self.assertEqual(
                canonical(owner.halls.planned[0]["rooflights"]),
                canonical(run.outputs["rooflights"].value),
            )

    def test_deferred_v2_tile_retains_full_glazing_plan_before_any_hall_geometry(self):
        row, neighbors = sources()[2]
        owner = FacadeDetails([row, *neighbors], view_for(row))
        owner.halls = HallDetails(owner.entrances, version="2")
        with (
            patch("tile_industrial_halls.demanded", return_value=False),
            patch(
                "tile_industrial_halls.structure",
                side_effect=AssertionError("deferred CSG"),
            ),
        ):
            owner(row, attributes(row["properties"]), 1)
        self.assertEqual(len(owner.halls.planned[0]["rooflights"]), 46)
        self.assertEqual(owner.halls.completed, [])
        self.assertEqual(owner.halls.current["hall_render"], "deferred")
        for version in (1, True, "3"):
            with self.assertRaises(ValueError):
                HallDetails(owner.entrances, version=version)

    def test_logical_fields_do_not_build_envelope_shell_or_panels(self):
        graph = prepare()
        for row, neighbors in sources()[:3]:
            with (
                patch(
                    "recipe_hall_operators.roof_surface",
                    side_effect=AssertionError("unrequested envelope"),
                ),
                patch(
                    "recipe_rooflight_operators.structure",
                    side_effect=AssertionError("unrequested CSG"),
                ),
                patch(
                    "recipe_rooflight_operators.closed",
                    side_effect=AssertionError("unrequested panels"),
                ),
            ):
                result = graph.evaluate(
                    inputs_for(row, neighbors), outputs=("rooflights",)
                )
            self.assertEqual(result.stats["evaluated_nodes"], 3)
            self.assertGreater(len(result.outputs["rooflights"].value), 0)

    def test_requested_panels_do_not_build_body_or_envelope(self):
        row, neighbors = sources()[2]
        with (
            patch(
                "recipe_hall_operators.roof_surface",
                side_effect=AssertionError("unrequested envelope"),
            ),
            patch(
                "recipe_rooflight_operators.structure",
                side_effect=AssertionError("unrequested CSG"),
            ),
        ):
            run = prepare().evaluate(inputs_for(row, neighbors), outputs=("panels",))
        self.assertEqual(run.stats["evaluated_nodes"], 4)
        panel_representation(run.outputs["panels"].value, 46)

    def test_actual_plans_and_full_mesh_equal_the_prepared_direct_construction(self):
        for row, neighbors in sources()[:3]:
            before = copy.deepcopy(row)
            expected = prepared(row, neighbors)
            run = prepare().evaluate(inputs_for(row, neighbors))
            self.assertEqual(run.stats["evaluated_nodes"], 6)
            mesh_equal(expected["body"], run.outputs["shell"].value.body)
            self.assertEqual(
                canonical(expected["fields"]),
                canonical(run.outputs["rooflights"].value),
            )
            self.assertEqual(
                canonical(metadata(expected["value"])),
                canonical(metadata(run.outputs["programme"].value.plan)),
            )
            for first, last in zip(
                closed(expected["fields"]), run.outputs["panels"].value, strict=True
            ):
                mesh_equal(first, last)
            self.assertEqual(row, before)
            with self.assertRaises(TypeError):
                run.outputs["rooflights"].value[0]["id"] = "changed"
            self.assertTrue(
                all(
                    not a.flags.writeable
                    for p in run.outputs["panels"].value
                    for a in p.values()
                )
            )

    def test_v1_and_v2_are_distinct_definitions_and_shell_types_with_same_room_ids(
        self,
    ):
        row, neighbors = sources()[1]
        inputs = inputs_for(row, neighbors)
        old, new = previous().evaluate(inputs), prepare().evaluate(inputs)
        self.assertNotEqual(old.definition_fingerprint, new.definition_fingerprint)
        self.assertNotEqual(old.outputs["shell"].type_id, new.outputs["shell"].type_id)
        self.assertEqual(
            canonical(metadata(old.outputs["programme"].value.plan)),
            canonical(metadata(new.outputs["programme"].value.plan)),
        )
        mesh_equal(
            old.outputs["envelope"].value.mesh, new.outputs["envelope"].value.mesh
        )

    def test_actual_glazed_roof_and_every_existing_access_state(self):
        for row, neighbors in sources()[:3]:
            run = prepare().evaluate(inputs_for(row, neighbors))
            shell = run.outputs["shell"].value
            plan = run.outputs["programme"].value.plan
            light_paths(shell.shell, run.outputs["rooflights"].value)
            fingerprints = {k: p.fingerprint for k, p in run.outputs.items()}
            for state in (0, 0.5, 1):
                mesh, chunks, _ = assemble(
                    run, view_for(row), opened=state, staff_angle=90
                )
                collision = manifold.Manifold.batch_boolean(
                    [
                        native(shell.body, shell.shell.vertex_roots),
                        *[native(p) for p in chunks],
                    ],
                    manifold.OpType.Add,
                )
                self.assertEqual(collision.status(), manifold.Error.NoError)
                loading_access(plan, collision, state)
                personnel_access(plan, collision, 90)
                hall_route(plan, collision)
                roof_coverage(mesh, row, run.outputs["rooflights"].value)
            self.assertEqual(
                fingerprints, {k: p.fingerprint for k, p in run.outputs.items()}
            )

    def test_retained_body_never_repeats_source_plan_or_csg_when_camera_state_changes(
        self,
    ):
        row, neighbors = sources()[2]
        inputs = inputs_for(row, neighbors)
        run = prepare().evaluate(inputs, outputs=("programme", "rooflights", "shell"))
        full = prepare().evaluate(inputs)
        self.assertEqual(run.stats["evaluated_nodes"], 5)
        with (
            patch(
                "recipe_rooflight_operators.plan",
                side_effect=AssertionError("repeated logical work"),
            ),
            patch(
                "recipe_rooflight_operators.structure",
                side_effect=AssertionError("repeated CSG"),
            ),
        ):
            for state in (0, 1):
                mesh, _, _ = assemble(run, view_for(row), opened=state)
                expected, _, _ = assemble(full, view_for(row), opened=state)
                mesh_equal(mesh, expected)

    def test_source_height_invalidates_glazing_shell_panels_but_keeps_owned_identities(
        self,
    ):
        row, neighbors = sources()[2]
        changed = copy.deepcopy(row)
        changed["properties"]["render_height"] = 6
        graph = prepare()
        old = graph.evaluate(inputs_for(row, neighbors))
        new = graph.evaluate(inputs_for(changed, neighbors))
        for name in ("rooflights", "shell", "panels"):
            self.assertNotEqual(
                old.outputs[name].fingerprint, new.outputs[name].fingerprint
            )
        self.assertEqual(
            [p["id"] for p in old.outputs["rooflights"].value],
            [p["id"] for p in new.outputs["rooflights"].value],
        )
        self.assertEqual(
            old.outputs["programme"].value.plan["room_id"],
            new.outputs["programme"].value.plan["room_id"],
        )

    def test_capacity_and_declared_bytes_reject_before_body_work(self):
        row, neighbors = sources()[0]
        graph = prepare()
        original = graph.definitions[ROOT]
        data = json.loads(canonical(original))
        data["nodes"]["rooflights"]["parameters"]["max_fields"] = 1

        def loader(key, limit):
            return canonical(data if key == ROOT else graph.definitions[key])

        with (
            patch(
                "recipe_rooflight_operators.structure",
                side_effect=AssertionError("unrequested CSG"),
            ),
            self.assertRaisesRegex(ValueError, "capacity"),
        ):
            prepare(loader=loader).evaluate(
                inputs_for(row, neighbors), outputs=("rooflights",)
            )
        with self.assertRaisesRegex(ValueError, "reservation"):
            prepare(budget=Budget(max_product_bytes=100)).evaluate(
                inputs_for(row, neighbors)
            )

    def test_actual_panel_dtype_nonfinite_and_count_reject(self):
        row, neighbors = sources()[2]
        parts = (
            prepare()
            .evaluate(inputs_for(row, neighbors), outputs=("panels",))
            .outputs["panels"]
            .value
        )
        with self.assertRaisesRegex(ValueError, "count"):
            panel_representation(parts, 1)
        for key, value in (
            ("vertices", parts[0]["vertices"].astype(np.float32)),
            ("uv", np.full_like(parts[0]["uv"], np.nan)),
        ):
            with self.assertRaises(ValueError):
                panel_representation([dict(parts[0], **{key: value})], 1)


if __name__ == "__main__":
    unittest.main()
