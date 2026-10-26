"""Actual hall relief: independent solids/access, aperture ownership and retained draw data."""

import unittest
from unittest.mock import patch

import manifold3d as manifold
import numpy as np
import shapely
from demand import View
from facade_entrances import EntranceDetails
from industrial_relief_mesh import direct, generic
from industrial_relief_plan import choose, footprint
from industrial_relief_products import ArticulatedDetails, demanded
from prepared_hall_tile import prepare_halls
from prepared_hall_tile_probe import source_tile
from structural_shell_geometry import native
from tile_buildings import construct
from verify_industrial_halls import hall_route, loading_access, personnel_access


class IndustrialRelief(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rows, cls.receipt, _context, cls.view = source_tile()
        cls.assets = prepare_halls(rows)
        cls.owner = ArticulatedDetails(cls.assets, cls.view)

    def test_all_actual_plans_are_bounded_and_keep_existing_apertures_clear(self):
        kinds = set()
        for identifier, parts in self.owner.reliefs.items():
            self.assertLessEqual(sum(p.plan.triangles for p in parts), 4096)
            for part in parts:
                p = part.plan
                kinds.add(p.kind)
                self.assertEqual(p.region.intersection(p.keepouts).area, 0)
                self.assertLess(
                    footprint(p)
                    .intersection(self.owner.source_rows[identifier]["polygon"])
                    .area,
                    1e-8,
                )
                self.assertTrue(all(not a.flags.writeable for a in part.full.values()))
                self.assertTrue(
                    all(
                        np.shares_memory(part.full[k], part.outer[k]) for k in part.full
                    )
                )
        self.assertEqual(kinds, {"portal-bays", "paired-bays", "ribbon-bays"})
        self.assertEqual(sum(map(len, self.owner.reliefs.values())), 39)
        self.assertEqual(len(self.owner.relief_rejected), 1)

    def test_direct_and_generic_solids_match_analytic_volume_area_and_metric_uvs(self):
        for parts in self.owner.reliefs.values():
            for part in parts:
                a, _count = direct(part.plan)
                b = generic(part.plan)
                solids = [native(a), native(b)]
                expected_volume = part.plan.region.area * part.plan.depth_m
                expected_area = (
                    2 * part.plan.region.area
                    + part.plan.region.length * part.plan.depth_m
                )
                for mesh, solid in zip((a, b), solids, strict=True):
                    self.assertEqual(solid.status(), manifold.Error.NoError)
                    self.assertAlmostEqual(solid.volume(), expected_volume, places=7)
                    self.assertAlmostEqual(
                        solid.surface_area(), expected_area, places=7
                    )
                    points, uv = (
                        mesh["vertices"][mesh["faces"]],
                        mesh["uv"][mesh["faces"]],
                    )
                    np.testing.assert_allclose(
                        np.linalg.norm(np.roll(points, 1, axis=1) - points, axis=2),
                        np.linalg.norm(np.roll(uv, 1, axis=1) - uv, axis=2),
                        atol=1e-9,
                    )
                    normals = np.cross(
                        points[:, 1] - points[:, 0], points[:, 2] - points[:, 0]
                    )
                    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
                    np.testing.assert_allclose(
                        normals, mesh["normals"][mesh["faces"][:, 0]], atol=1e-10
                    )
                self.assertLess((solids[0] - solids[1]).volume(), 1e-8)
                self.assertLess((solids[1] - solids[0]).volume(), 1e-8)

    def test_new_closed_parts_preserve_all_actual_gate_personnel_and_internal_routes(
        self,
    ):
        for identifier, parts in self.owner.reliefs.items():
            actor = self.owner.instances[identifier]
            shell = actor.asset.run.outputs["shell"].value
            for opened, angle in ((0, 0), (0.5, 90), (1, 90)):
                draw = actor.update(self.view, opened=opened, staff_angle=angle)
                collision = manifold.Manifold.batch_boolean(
                    [
                        native(draw.body, shell.shell.vertex_roots),
                        *[native(p) for p in draw.physical_parts],
                        *[native(p.full) for p in parts],
                    ],
                    manifold.OpType.Add,
                )
                self.assertEqual(collision.status(), manifold.Error.NoError)
                loading_access(actor.asset.plan, collision, opened)
                personnel_access(actor.asset.plan, collision, angle)
                hall_route(actor.asset.plan, collision)

    def test_covered_backs_actually_meet_opaque_source_wall(self):
        for identifier, parts in self.owner.reliefs.items():
            asset = self.assets.assets[identifier]
            host = native(
                asset.body, asset.run.outputs["shell"].value.shell.vertex_roots
            )
            for part in parts:
                raw, _count = direct(part.plan)
                caps = raw["vertices"][raw["faces"]][
                    raw["normals"][raw["faces"][:, 0], 2] > 0.99
                ]
                t = np.asarray(part.plan.tangent)
                outward = np.r_[[t[1], -t[0]], 0]
                for local in caps.mean(axis=1):
                    point = np.r_[np.asarray(part.plan.origin) + local[0] * t, local[1]]
                    self.assertTrue(
                        host.ray_cast(point - outward * 0.01, point + outward * 0.01)
                    )

    def test_whole_tile_keeps_all_sources_and_uses_only_prepared_geometry(self):
        with patch(
            "industrial_relief_products.products",
            side_effect=AssertionError("warm mesh emission"),
        ):
            owner = self.owner
            mesh, source = construct(
                "Grohnde",
                prepared=(self.assets.rows, self.receipt),
                ordinary_builder=owner,
            )
        self.assertEqual(source["generated"], 37)
        self.assertEqual(source["omissions"], [])
        self.assertGreater(len(mesh["faces"]), 84801)
        for member in source["members"]:
            row = self.owner.source_rows[member["id"]]
            self.assertEqual(member["source_properties"], row["properties"])
            self.assertEqual(member["footprint_wkb"], row["polygon"].wkb_hex)

    def test_far_selection_and_zero_budget_do_not_change_physical_parts(self):
        part = next(iter(self.owner.reliefs.values()))[0]
        centre = part.wall_bounds.mean(axis=0)
        self.assertTrue(
            demanded(part, View(centre + [1, -2, 2], centre, width=1920, height=1080))
        )
        self.assertFalse(
            demanded(
                part,
                View(centre + [10000, -10000, 10000], centre, width=1920, height=1080),
            )
        )
        identifier = next(iter(self.assets.assets))
        row, asset = self.owner.source_rows[identifier], self.assets.assets[identifier]
        plans, rejected = choose(
            asset, row, self.owner.ordinary.facades.entrances.clear, max_triangles=0
        )
        self.assertEqual(plans, ())
        self.assertTrue(all("capacity" in r["reason"] for r in rejected))
        for invalid in (True, -1):
            with self.assertRaises(ValueError):
                choose(
                    asset,
                    row,
                    self.owner.ordinary.facades.entrances.clear,
                    max_triangles=invalid,
                )

    def test_real_neighbor_height_and_source_wings_reject_before_emission(self):
        identifier, parts = next(iter(self.owner.reliefs.items()))
        row, asset = self.owner.source_rows[identifier], self.assets.assets[identifier]
        support = footprint(parts[0].plan)
        for low, expected in ((0, True), (100, False)):
            other = dict(
                row,
                id="neighbor",
                polygon=support,
                properties={"render_height": low + 5, "render_min_height": low},
            )
            rows = [row, other]
            clear = EntranceDetails(
                rows, shapely.STRtree([r["polygon"] for r in rows])
            ).clear
            _plans, rejected = choose(asset, row, clear)
            self.assertEqual(any("neighbor" in r["reason"] for r in rejected), expected)
        wing = dict(row, polygon=row["polygon"].union(support))
        _plans, rejected = choose(asset, wing, lambda *args, **kw: True)
        self.assertTrue(any("wing" in r["reason"] for r in rejected))


if __name__ == "__main__":
    unittest.main()
