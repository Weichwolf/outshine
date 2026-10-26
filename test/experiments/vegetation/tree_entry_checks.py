"""Read-only fixed rows, original controls, geometry and source-free cache consumption."""

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import Blade, PlantPlan
from plant_experiment import check_plant, sapling
from tree_entry import IDENTITY, build, compare_rows, load, reference, wood_mesh


class TreeEntryChecks(unittest.TestCase):
    def test_existing_recipe_literal_counts_and_complete_readonly_fixture(self):
        metadata, rows = reference()
        plant = sapling(17)
        self.assertEqual(plant.identity, IDENTITY)
        self.assertEqual(plant.recipe(), metadata["recipe"])
        self.assertEqual(len(rows["axis_ids"]), 64)
        self.assertEqual(len(rows["leaf_ids"]), 571)
        self.assertEqual(sum(a.nbytes for a in rows.values()), 93419)
        np.testing.assert_array_equal(
            np.bincount(rows["leaf_variants"]), [71, 72, 72, 72, 71, 71, 71, 71]
        )
        report = check_plant(plant)
        self.assertEqual(report["axes"], 64)
        self.assertEqual(report["leaves"], 571)
        self.assertLess(report["max_attachment_error_m"], 5e-16)
        compare_rows(plant.tables(64, 571), rows)

    def test_saved_recipe_reversed_partial_requests_and_actual_parent_owner_links(self):
        metadata, expected = reference()
        plan = PlantPlan.from_recipe(json.loads(json.dumps(metadata["recipe"])))
        self.assertIsNone(plan.tables(63, 571))
        self.assertFalse(plan._axes or plan._leaves)
        self.assertIsNone(plan.tables(64, 570))
        self.assertFalse(plan._axes or plan._leaves)
        for path in reversed(tuple(plan.paths())):
            axis = plan.axis(path)
            if path:
                self.assertEqual(
                    expected["axis_parent_ids"][axis.id], plan.axis(path[:-1]).id
                )
            group = plan.leaves(path, 571)
            if group is None:
                continue
            mask = expected["leaf_axis_ids"] == axis.id
            for name, values in group.items():
                if values.dtype.kind == "f":
                    np.testing.assert_allclose(
                        values, expected["leaf_" + name][mask], rtol=0, atol=1e-14
                    )
                else:
                    np.testing.assert_array_equal(
                        values, expected["leaf_" + name][mask]
                    )
        actual = plan.tables(64, 571)
        compare_rows(actual, expected)
        roots = (
            actual["leaf_origins"]
            - plan.blades[0].petiole * actual["leaf_frames"][:, :, 1]
        )
        np.testing.assert_allclose(roots, actual["leaf_nodes"], rtol=0, atol=5e-16)
        wrong = {k: v.copy() for k, v in expected.items()}
        wrong["leaf_axis_ids"][0] += 1
        with self.assertRaises(AssertionError):
            compare_rows(actual, wrong)

    def test_full_axis_mesh_orientation_closed_edges_and_unmodified_rows(self):
        metadata, rows = reference()
        before = {k: v.copy() for k, v in rows.items()}
        mesh = wood_mesh(rows, metadata["geometry"])
        self.assertEqual(set(mesh["axis_ids"]), set(range(64)))
        p = mesh["vertices"][mesh["faces"]]
        cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        self.assertTrue(
            (
                np.einsum("ij,ikj->ik", cross, mesh["normals"].reshape(-1, 3, 3)) > 0
            ).all()
        )
        faces = mesh["faces"]
        edges = np.sort(
            np.concatenate((faces[:, :2], faces[:, 1:], faces[:, ::2])), axis=1
        )
        _, counts = np.unique(edges, axis=0, return_counts=True)
        np.testing.assert_array_equal(counts, np.full(len(counts), 2))
        for name in rows:
            np.testing.assert_array_equal(rows[name], before[name])

    def test_fresh_cache_has_complete_rows_and_never_calls_source_constructors(self):
        _, expected = reference()
        with tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-tree-entry-test."
        ) as tmp:
            output = Path(tmp) / "products"
            with contextlib.redirect_stdout(io.StringIO()):
                build(output)
            with (
                patch.object(
                    PlantPlan,
                    "from_recipe",
                    side_effect=AssertionError("source plan on hit"),
                ),
                patch.object(
                    Blade, "mesh", side_effect=AssertionError("leaf meshing on hit")
                ),
                patch(
                    "tree_entry.axis_mesh",
                    side_effect=AssertionError("wood meshing on hit"),
                ),
                patch.object(
                    np.random, "default_rng", side_effect=AssertionError("RNG on hit")
                ),
            ):
                metadata, products = load(output)
            compare_rows(products["tables.npz"], expected)
            self.assertEqual(metadata["plan_identity"], IDENTITY)
            self.assertEqual(len(products), 10)
            self.assertEqual(metadata["instanced_triangles"], 174060)
            for i in range(8):
                self.assertEqual(len(products[f"blade-{i}.npz"]["faces"]), 260)
            self.assertEqual(
                json.loads((output / "recipe.json").read_text())["version"],
                "branching-plant-2",
            )


if __name__ == "__main__":
    unittest.main()
