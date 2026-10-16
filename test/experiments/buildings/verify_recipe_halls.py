"""Loaded actual hall recipes retain the direct generator and avoid unrequested surfaces."""

import copy
import unittest
from dataclasses import replace
from unittest.mock import patch

import manifold3d as manifold
import numpy as np
from facade_parts import append
from industrial_hall_details import products
from industrial_hall_plan import metadata
from recipe_graph_data import Budget, canonical
from recipe_hall_operators import inputs_for, prepare
from structural_shell_geometry import native
from verify_industrial_halls import (
    construct,
    hall_route,
    loading_access,
    personnel_access,
    roof_preserved,
    sources,
    view_for,
)


def mesh_equal(first, second):
    if set(first) != set(second):
        raise ValueError("Recipe mesh streams changed")
    for key, value in first.items():
        if value.dtype != second[key].dtype:
            raise ValueError("Recipe mesh dtype changed")
        np.testing.assert_array_equal(value, second[key])


def assembly(run, view, opened=1):
    programme = run.outputs["programme"].value.plan
    value = dict(programme, entry=dict(programme["entry"], angle_deg=90))
    shell = run.outputs["shell"].value
    chunks, _ = products(value, 1, opened, view)
    collision = manifold.Manifold.batch_boolean(
        [native(shell.body, shell.shell.vertex_roots), *[native(c) for c in chunks]],
        manifold.OpType.Add,
    )
    return append(shell.body, chunks), collision, value


class RecipeHalls(unittest.TestCase):
    def test_all_real_halls_same_plan_and_complete_geometry_with_actual_function(self):
        graph = prepare()
        for row, neighbors in sources()[:3]:
            before = copy.deepcopy(row)
            inputs = inputs_for(row, neighbors)
            run = graph.evaluate(inputs)
            _owner, direct, info, plan, _shell, _chunks, _collision = construct(
                row, neighbors
            )
            self.assertEqual(
                canonical(metadata(run.outputs["programme"].value.plan)),
                canonical(metadata(plan)),
            )
            self.assertEqual(
                canonical(run.outputs["programme"].value.facades),
                canonical(info["facade_plan"]),
            )
            mesh, collision, value = assembly(run, view_for(row))
            mesh_equal(direct, mesh)
            loading_access(value, collision, 1)
            personnel_access(value, collision, 90)
            hall_route(value, collision)
            roof_preserved(mesh, row)
            self.assertEqual(row, before)
            self.assertEqual(run.stats["loaded_definitions"], 2)
            self.assertEqual(run.stats["evaluated_nodes"], 4)

    def test_programme_only_never_builds_heightfield_mesh_or_csg(self):
        row, neighbors = sources()[2]
        graph = prepare()
        inputs = inputs_for(row, neighbors)
        with (
            patch(
                "recipe_hall_operators.roof_surface",
                side_effect=AssertionError("unrequested roof mesh"),
            ),
            patch(
                "recipe_hall_operators.heightfield_envelope",
                side_effect=AssertionError("unrequested envelope"),
            ),
            patch(
                "recipe_hall_operators.structure",
                side_effect=AssertionError("unrequested CSG"),
            ),
        ):
            logical = graph.evaluate(inputs, outputs=("programme",))
        full = graph.evaluate(inputs)
        self.assertEqual(logical.stats["evaluated_nodes"], 2)
        self.assertEqual(
            logical.outputs["programme"].fingerprint,
            full.outputs["programme"].fingerprint,
        )
        self.assertEqual(logical.instance_fingerprint, full.instance_fingerprint)

    def test_world_gate_state_stays_outside_definition_and_static_products(self):
        row, neighbors = sources()[2]
        run = prepare().evaluate(inputs_for(row, neighbors))
        fingerprints = [p.fingerprint for p in run.outputs.values()]
        for state in (0, 0.5, 1):
            _mesh, collision, value = assembly(run, view_for(row), state)
            loading_access(value, collision, state)
            self.assertEqual(
                [p.fingerprint for p in run.outputs.values()], fingerprints
            )
        self.assertEqual(run.outputs["programme"].value.plan["entry"]["angle_deg"], 0)

    def test_neighbor_content_invalidates_programme_shell_but_reuses_roof_envelope(
        self,
    ):
        row, neighbors = sources()[1]
        self.assertTrue(neighbors)
        graph = prepare()
        before = graph.evaluate(inputs_for(row, neighbors))
        changed = copy.deepcopy(neighbors)
        changed[0]["properties"]["render_height"] = 12
        after = graph.evaluate(inputs_for(row, changed))
        self.assertEqual(before.definition_fingerprint, after.definition_fingerprint)
        self.assertEqual(
            before.outputs["envelope"].fingerprint,
            after.outputs["envelope"].fingerprint,
        )
        for name in ("programme", "shell"):
            self.assertNotEqual(
                before.outputs[name].fingerprint, after.outputs[name].fingerprint
            )
        self.assertNotEqual(before.instance_fingerprint, after.instance_fingerprint)

    def test_snapshot_inputs_read_only_plans_and_neighbor_permutation(self):
        row, neighbors = sources()[1]
        inputs = inputs_for(row, neighbors)
        reverse = inputs_for(row, list(reversed(neighbors)))
        self.assertEqual(inputs["site"].fingerprint, reverse["site"].fingerprint)
        row["properties"]["render_height"] = 99
        self.assertNotEqual(inputs["source"].value["properties"]["render_height"], 99)
        run = prepare().evaluate(inputs)
        with self.assertRaises(TypeError):
            run.outputs["programme"].value.plan["gate"]["width_m"] = 20
        for array in run.outputs["shell"].value.body.values():
            self.assertFalse(array.flags.writeable)

    def test_invalid_frame_and_ineligible_actual_source_never_produce_checked_shell(
        self,
    ):
        row, neighbors = sources()[2]
        inputs = inputs_for(row, neighbors)
        site = inputs["site"]
        forged = replace(
            site, metadata=dict(site.metadata) | {"frame_id": "different-local-origin"}
        )
        with (
            patch(
                "recipe_hall_operators.structure",
                side_effect=AssertionError("invalid-frame CSG"),
            ),
            self.assertRaisesRegex(ValueError, "frames"),
        ):
            prepare().evaluate(inputs | {"site": forged})
        row, neighbors = sources()[3]
        with (
            patch(
                "recipe_hall_operators.structure",
                side_effect=AssertionError("invalid access CSG"),
            ),
            self.assertRaisesRegex(ValueError, "loading cell"),
        ):
            prepare().evaluate(inputs_for(row, neighbors))

    def test_accounted_output_budget_precedes_all_physical_work(self):
        row, neighbors = sources()[0]
        graph = prepare(budget=Budget(max_product_bytes=1024))
        with (
            patch(
                "recipe_hall_operators.choose",
                side_effect=AssertionError("over-budget work"),
            ),
            self.assertRaisesRegex(ValueError, "reservation"),
        ):
            graph.evaluate(inputs_for(row, neighbors))


if __name__ == "__main__":
    unittest.main()
