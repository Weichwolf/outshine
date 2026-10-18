"""Actual wall/opening attachment, closed volume, early work and route negatives."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from bay_facade_access import check_routes, unobstructed
from bay_facade_controls import cases
from bay_facade_mesh import expand
from bay_facade_plan import PROGRAMMES, plan_bays
from bay_wall_domains import domains
from building_mesh import solid
from building_plan import FLOOR_M
from clad_facade_mesh import surface
from facade_frame_geometry import placed
from structural_shell_geometry import native


def source_apertures(test, solid, layer, terrace):
    mass = terrace.prepared.mass
    core = mass.core
    values = [
        (
            np.asarray(core.origin_xy)
            + w.centre[0] * np.asarray(core.across_xy)
            + w.centre[1] * np.asarray(core.along_xy),
            w.bottom_m,
            w.width_m,
            w.height_m,
        )
        for w in (*terrace.prepared.facade.windows, *terrace.portals)
    ]
    values.append(
        (
            np.asarray(mass.entry.centre),
            mass.bottom_m + FLOOR_M,
            1.10,
            mass.entry.height_m,
        )
    )
    tangent = np.asarray(layer.tangent)
    normal = np.array([tangent[1], -tangent[0]])
    for point, low, width, height in values:
        relative = point - layer.origin_xy
        if abs(float(relative @ normal) - 0.005) > 1e-7:
            continue
        u, v = float(relative @ tangent), low - layer.base_m
        opening = shapely.box(u - width / 2, v, u + width / 2, v + height)
        test.assertLess(layer.region.intersection(opening).area, 1e-9)
        for dx in (-0.35, 0, 0.35):
            for dz in (0.15, 0.5, 0.85):
                p = [u + dx * width, v + dz * height]
                test.assertEqual(
                    len(solid.ray_cast([*p, -0.01], [*p, layer.depth_m + 0.01])), 0
                )


class BayFacadeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = cases()
        cls.plans = [
            (
                row,
                label,
                terrace,
                plan_bays(terrace.prepared, style, portals=terrace.portals),
            )
            for row, label, terrace in cls.cases
            for style in PROGRAMMES
        ]

    def test_real_walls_and_openings_have_stable_source_identities(self):
        for row, _label, terrace, planned in self.plans:
            self.assertEqual(terrace.prepared.mass.source_wkb, row["polygon"].wkb)
            self.assertGreater(len(planned.cells), 0)
            original = {w.identifier for w in terrace.prepared.facade.windows}
            for cell in planned.cells:
                self.assertTrue(set(cell.window_ids) <= original)
                self.assertGreaterEqual(len(cell.window_ids), 2)
            reversed_prepared = replace(
                terrace.prepared,
                facade=replace(
                    terrace.prepared.facade,
                    windows=tuple(reversed(terrace.prepared.facade.windows)),
                ),
            )
            actual = plan_bays(
                reversed_prepared,
                planned.programme,
                portals=tuple(reversed(terrace.portals)),
            )
            self.assertEqual(planned, actual)

    def test_every_layer_is_closed_positive_and_preserves_protected_apertures(self):
        for _row, _label, terrace, planned in self.plans:
            for cell in planned.cells:
                for layer in cell.layers:
                    product = surface(layer, "closed")
                    actual = native(product)
                    source_apertures(self, actual, layer, terrace)
                    self.assertAlmostEqual(
                        actual.volume(), layer.region.area * layer.depth_m, delta=1e-7
                    )
                    self.assertLess(
                        layer.region.intersection(
                            shapely.from_wkb(layer.protected_wkb)
                        ).area,
                        1e-10,
                    )
                    for polygon in shapely.get_parts(
                        shapely.from_wkb(layer.protected_wkb)
                    ):
                        if not polygon.intersects(shapely.box(*layer.region.bounds)):
                            continue
                        p = polygon.representative_point()
                        self.assertEqual(
                            len(
                                actual.ray_cast(
                                    [p.x, p.y, -0.01], [p.x, p.y, layer.depth_m + 0.01]
                                )
                            ),
                            0,
                        )

    def test_actual_back_surfaces_attach_to_source_wall_not_room_or_opening_voids(self):
        for _row, _label, terrace, planned in self.plans:
            mass = terrace.prepared.mass
            polygons = [shapely.from_wkb(s.footprint_wkb) for s in mass.storeys]
            for cell in planned.cells:
                for layer in cell.layers:
                    raw = surface(layer, "closed")
                    mesh = placed(raw, layer, layer.material)
                    back = np.all(raw["vertices"][raw["faces"], 2] == 0, axis=1)
                    samples = mesh["vertices"][mesh["faces"]][back].mean(axis=1)
                    for point in samples:
                        self.assertTrue(
                            any(
                                s.bottom_m - 1e-9 <= point[2] <= s.top_m + 1e-9
                                and p.buffer(1e-8).covers(shapely.Point(point[:2]))
                                and p.boundary.distance(shapely.Point(point[:2])) < 0.01
                                for s, p in zip(mass.storeys, polygons, strict=True)
                            )
                        )

    def test_existing_approaches_entries_and_terrace_routes_remain_clear(self):
        for _row, _label, terrace, planned in self.plans:
            result = check_routes(planned, terrace.prepared, terrace.portals)
            self.assertGreaterEqual(result["routes"], 2)
        storage = [], [], [], []
        solid(storage, shapely.box(-0.1, -0.1, 0.1, 0.1), 0.20, 1.5, 1)
        blocked = {"vertices": np.array(storage[0]), "faces": np.array(storage[1])}
        with self.assertRaisesRegex(ValueError, "obstructs"):
            unobstructed(blocked, shapely.LineString([(-1, 0), (1, 0)]), 0)

    def test_front_and_outer_are_exact_closed_subsets_without_hidden_expansion(self):
        for _row, _label, _terrace, planned in self.plans:
            for cell in (
                planned.cells[0],
                planned.cells[len(planned.cells) // 2],
                planned.cells[-1],
            ):
                for layer in cell.layers:
                    closed = surface(layer, "closed")
                    with patch(
                        "clad_facade_mesh.sides",
                        side_effect=AssertionError("hidden sides"),
                    ):
                        front = surface(layer, "front")
                    xyz = closed["vertices"][closed["faces"]]
                    keep = np.all(xyz[:, :, 2] == layer.depth_m, axis=1)
                    for key in ("vertices", "normals", "uv"):
                        np.testing.assert_array_equal(
                            closed[key][closed["faces"]][keep],
                            front[key][front["faces"]],
                        )
                    self.assertEqual(len(front["faces"]), layer.triangles("front"))
                    outer = surface(layer, "outer")
                    keep = ~np.all(xyz[:, :, 2] == 0, axis=1)
                    for key in ("vertices", "normals", "uv"):
                        np.testing.assert_array_equal(
                            closed[key][closed["faces"]][keep],
                            outer[key][outer["faces"]],
                        )
            with patch(
                "bay_facade_mesh.surface", side_effect=AssertionError("unselected")
            ):
                mesh, ids = expand(planned, [])
                self.assertEqual(len(mesh["faces"]), 0)
                self.assertEqual(len(ids), 0)
                with self.assertRaises(ValueError):
                    expand(planned, ["foreign"])

    def test_shifted_window_portal_wall_entry_and_invalid_capacity_reject(self):
        _row, _label, terrace = self.cases[1]
        prepared = terrace.prepared
        window = prepared.facade.windows[0]
        bad = replace(window, centre=tuple(np.asarray(window.centre) + [0.04, 0.07]))
        with self.assertRaisesRegex(ValueError, "wall edge"):
            plan_bays(
                replace(
                    prepared,
                    facade=replace(
                        prepared.facade, windows=(bad, *prepared.facade.windows[1:])
                    ),
                ),
                "ribbon-fields",
                portals=terrace.portals,
            )
        entry = replace(
            prepared.local.entry,
            centre=tuple(np.asarray(prepared.local.entry.centre) + [0.04, 0.07]),
        )
        with self.assertRaisesRegex(ValueError, "ground wall"):
            plan_bays(
                replace(prepared, local=replace(prepared.local, entry=entry)),
                "ribbon-fields",
            )
        for value in (False, 1.0, float("inf"), -1):
            with self.assertRaises(ValueError):
                plan_bays(prepared, "ribbon-fields", max_triangles=value)
        empty = plan_bays(
            prepared, "ribbon-fields", portals=terrace.portals, max_triangles=0
        )
        self.assertFalse(empty.cells)
        self.assertTrue(empty.rejected)
        if terrace.portals:
            portal = replace(
                terrace.portals[0],
                centre=tuple(np.asarray(terrace.portals[0].centre) + [0.04, 0.07]),
            )
            with self.assertRaisesRegex(ValueError, "wall edge"):
                plan_bays(
                    prepared, "ribbon-fields", portals=(portal, *terrace.portals[1:])
                )
        _walls, rejected = domains(prepared, portals=terrace.portals, outward_m=100)
        self.assertTrue(rejected)
        shifted = shapely.affinity.translate(
            shapely.from_wkb(prepared.local.storeys[1].footprint_wkb),
            xoff=0.04,
            yoff=0.07,
        )
        storeys = (
            prepared.local.storeys[0],
            replace(prepared.local.storeys[1], footprint_wkb=shifted.wkb),
            *prepared.local.storeys[2:],
        )
        with self.assertRaisesRegex(ValueError, "wall edge"):
            plan_bays(
                replace(prepared, local=replace(prepared.local, storeys=storeys)),
                "ribbon-fields",
                portals=terrace.portals,
            )


if __name__ == "__main__":
    unittest.main()
