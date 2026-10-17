"""Actual intake solids, connected supports, finite passage and provider-preserving controls."""

import unittest
from dataclasses import replace

import manifold3d as manifold
import numpy as np
import shapely
from cooling_tower_costs import compare_support
from cooling_tower_details import refine
from cooling_tower_mesh import components, draw_parts
from cooling_tower_plan import plan
from cooling_tower_probe import sources
from outshine_ref.buildings.modelling import Shape
from outshine_ref.buildings.modelling.radial_mesh import radial_mesh
from structural_shell_geometry import native
from tile_buildings import construct
from verify_recipe_halls import mesh_equal


def passage(programme, bay):
    radius = programme.source.knots[0][1]
    angle = 2 * np.pi * (bay + 0.5) / programme.bays
    direction = np.array([np.cos(angle), np.sin(angle)])
    centre = np.asarray(programme.source.centre_xy)
    polygon = shapely.LineString(
        [
            centre + direction * (radius - 4),
            centre + direction * (radius + 2),
        ]
    ).buffer(0.35, quad_segs=8)
    low = programme.source.knots[0][0] + 0.2
    raw = Shape.from_polygon(polygon, elevation=low).extrude(2).emit()
    # General modelling uses CGA Y-up; the tested assembly uses local ENU Z-up.
    return native(dict(raw, vertices=raw["vertices"][:, [0, 2, 1]] * [1, -1, 1]))


class CoolingTowers(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows, cls.receipt, cls.view, cls.cases = sources()
        cls.parts = [components(p) for _r, p in cls.cases]
        cls.solids = [
            manifold.Manifold.batch_boolean(
                [native(m) for m in c.values()], manifold.OpType.Add
            )
            for c in cls.parts
        ]

    def test_real_components_connect_with_exact_height_and_footprint(self):
        self.assertEqual(len(self.cases), 2)
        for (row, programme), parts, solid in zip(
            self.cases, self.parts, self.solids, strict=True
        ):
            self.assertEqual(len(parts), 3 + 2 * programme.bays)
            self.assertEqual(len(solid.decompose()), 1)
            self.assertGreater(solid.volume(), 0)
            low, high = programme.source.knots[0][0], programme.source.knots[-1][0]
            self.assertEqual(
                min(m["vertices"][:, 2].min() for m in parts.values()), low
            )
            self.assertEqual(
                max(m["vertices"][:, 2].max() for m in parts.values()), high
            )
            for mesh in parts.values():
                physical = native(mesh)
                self.assertGreater(physical.volume(), 0)
                points = shapely.points(mesh["vertices"][:, :2])
                self.assertTrue(
                    shapely.covers(row["polygon"].buffer(1e-9), points).all()
                )
                for array in mesh.values():
                    self.assertTrue(np.isfinite(array).all())
                    self.assertFalse(array.flags.writeable)
            for support in programme.supports:
                mesh = parts[support.identifier]
                expected = support.width_m**2 * (support.end[2] - support.start[2])
                self.assertAlmostEqual(native(mesh).volume(), expected, places=9)

    def test_each_real_bay_leaves_a_continuously_swept_finite_intake_passage(self):
        for (_row, programme), solid in zip(self.cases, self.solids, strict=True):
            old = native(radial_mesh(programme.source, error_m=0.2).mesh)
            for bay in range(programme.bays):
                swept = passage(programme, bay)
                self.assertGreater(swept.volume(), 1)
                self.assertEqual((solid ^ swept).volume(), 0)
                self.assertGreater((old ^ swept).volume(), 0.1)

    def test_smooth_normals_preserve_geometry_uv_material_and_physical_product(self):
        for (_row, programme), physical in zip(self.cases, self.parts, strict=True):
            draw = draw_parts(programme, physical)
            for key in physical:
                if key != programme.shell.identifier:
                    self.assertIs(draw[key], physical[key])
                    continue
                before, after = physical[key], draw[key]
                for stream in before.keys() - {"normals"}:
                    self.assertIs(before[stream], after[stream])
                self.assertFalse(after["normals"].flags.writeable)
                np.testing.assert_allclose(
                    np.linalg.norm(after["normals"], axis=1), 1, atol=1e-14
                )
                xyz = after["vertices"][after["faces"]]
                geometric = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
                self.assertTrue(
                    (
                        np.einsum(
                            "ti,tji->tj", geometric, after["normals"][after["faces"]]
                        )
                        > 0
                    ).all()
                )
                self.assertGreater(
                    abs(after["normals"] - before["normals"]).max(), 0.001
                )

    def test_plan_replays_and_elevated_sources_keep_their_complete_interval(self):
        for _row, programme in self.cases:
            self.assertEqual(plan(programme.source), programme)
            shifted = replace(
                programme.source,
                knots=tuple((z + 23, r, i) for z, r, i in programme.source.knots),
            )
            elevated = plan(shifted)
            meshes = components(elevated)
            self.assertEqual(
                min(m["vertices"][:, 2].min() for m in meshes.values()), 23
            )
            self.assertEqual(
                max(m["vertices"][:, 2].max() for m in meshes.values()), 161
            )
        source = self.cases[0][1].source
        for invalid in (
            replace(source, knots=((0, 6, 5), (138, 4, 3))),
            replace(source, knots=((0, 50, 49), (20, 30, 29))),
            replace(source, knots=((0, 50, 0), (138, 30, 0))),
            replace(source, knots=((0, 50, 49), (138, 51, 50))),
        ):
            with self.assertRaises(ValueError):
                plan(invalid)

    def test_optional_recipe_keeps_all_sources_and_other_families_exact(self):
        _a, before = construct("Grohnde", prepared=(self.rows, self.receipt))
        _b, after = construct(
            "Grohnde", prepared=(self.rows, self.receipt), radial_builder=refine
        )
        self.assertEqual(after["generated"], 37)
        self.assertEqual(after["omissions"], [])
        for key in ("counts", "material_definitions", "delivered_building_polygons"):
            self.assertEqual(after[key], before[key])
        for a, b in zip(before["members"], after["members"], strict=True):
            for key in a.keys() - {"prototype"}:
                self.assertEqual(a[key], b[key])
        for row in self.rows:
            if row["layer"] != "building" or any(
                row["id"] == r["id"] for r, _p in self.cases
            ):
                continue
            a, _ = construct("Grohnde", prepared=((row,), self.receipt))
            b, _ = construct(
                "Grohnde", prepared=((row,), self.receipt), radial_builder=refine
            )
            mesh_equal(a, b)

    def test_unfitting_explicit_cooling_source_keeps_a_recorded_coarse_body(self):
        row = self.cases[0][0]
        short = dict(
            row,
            properties=dict(
                row["properties"], render_height=20, **{"class": "cooling_tower"}
            ),
        )
        mesh, source = construct(
            "Grohnde", prepared=((short,), self.receipt), radial_builder=refine
        )
        self.assertEqual(source["generated"], 1)
        self.assertEqual(source["omissions"], [])
        self.assertIn("cooling_intake_declined", source["members"][0]["prototype"])
        np.testing.assert_array_equal(
            [mesh["vertices"][:, 2].min(), mesh["vertices"][:, 2].max()], [0, 20]
        )

    def test_general_shape_supports_meet_the_same_solid_and_metric_contract(self):
        for _row, programme in self.cases:
            for support in programme.supports:
                compare_support(support)


if __name__ == "__main__":
    unittest.main()
