"""Complete offline provider tile, source retention and independent hall function."""

import unittest
from unittest.mock import patch

import manifold3d as manifold
from prepared_hall import HallInstance
from prepared_hall_tile import RetainedDetails, prepare_halls
from prepared_hall_tile_probe import source_tile
from recipe_graph_data import canonical
from structural_shell_geometry import native
from tile_buildings import construct
from tile_industrial_halls import HallDetails
from tile_stepped import SteppedDetails
from verify_industrial_halls import hall_route, loading_access, personnel_access
from verify_industrial_rooflights import light_paths


class PreparedHallTile(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows, cls.receipt, cls.context, cls.view = source_tile()
        cls.prepared = prepare_halls(cls.rows)

    def test_whole_tile_preserves_every_source_and_adds_deferred_glazed_halls(self):
        rows = self.prepared.rows
        baseline = SteppedDetails(rows, self.view)
        baseline.facades.halls = HallDetails(baseline.facades.entrances, version="2")
        old_mesh, old = construct(
            "Grohnde",
            prepared=(rows, self.receipt),
            ordinary_builder=baseline,
        )
        cached = RetainedDetails(self.prepared, self.view)
        with patch(
            "recipe_rooflight_operators.structure",
            side_effect=AssertionError("warm CSG"),
        ):
            mesh, current = construct(
                "Grohnde",
                prepared=(rows, self.receipt),
                ordinary_builder=cached,
            )
        for key in (
            "generated",
            "delivered_building_polygons",
            "omissions",
            "counts",
            "material_definitions",
        ):
            self.assertEqual(canonical(current[key]), canonical(old[key]))
        self.assertEqual(current["generated"], 37)
        self.assertEqual(current["omissions"], [])
        for first, last in zip(old["members"], current["members"], strict=True):
            for key in first.keys() - {"prototype"}:
                self.assertEqual(canonical(first[key]), canonical(last[key]))
        self.assertEqual(len(self.prepared.assets), 8)
        self.assertEqual(sum(r["fields"] for r in cached.completed), 228)
        self.assertGreater(len(mesh["faces"]), len(old_mesh["faces"]))
        full = RetainedDetails(self.prepared, self.view, reference_full_fields=True)
        full_mesh, _ = construct(
            "Grohnde", prepared=(rows, self.receipt), ordinary_builder=full
        )
        self.assertEqual(len(full_mesh["faces"]) - len(mesh["faces"]), 228 * 24)

    def test_all_eight_actual_halls_have_free_routes_and_owned_lightwells(self):
        for asset in self.prepared.assets.values():
            draw = HallInstance(asset).update(self.view, opened=1, staff_angle=90)
            shell = asset.run.outputs["shell"].value
            collision = manifold.Manifold.batch_boolean(
                [
                    native(draw.body, shell.shell.vertex_roots),
                    *[native(p) for p in draw.physical_parts],
                ],
                manifold.OpType.Add,
            )
            self.assertEqual(collision.status(), manifold.Error.NoError)
            loading_access(asset.plan, collision, 1)
            personnel_access(asset.plan, collision, 90)
            hall_route(asset.plan, collision)
            light_paths(shell.shell, asset.run.outputs["rooflights"].value)

    def test_snapshot_cannot_be_mutated_or_used_with_foreign_same_id_row(self):
        source = next(r for r in self.prepared.rows if r["id"] in self.prepared.assets)
        with self.assertRaises(TypeError):
            source["properties"]["render_height"] = 123
        foreign = dict(source, properties=dict(source["properties"], render_height=123))
        with self.assertRaisesRegex(ValueError, "immutable prepared source snapshot"):
            RetainedDetails(self.prepared, self.view)(foreign, {}, 1)


if __name__ == "__main__":
    unittest.main()
