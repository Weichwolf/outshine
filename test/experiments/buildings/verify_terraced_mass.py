"""Actual mass/core controls and independent closed-volume/surface/section checks."""

import copy
import unittest
from dataclasses import FrozenInstanceError

import numpy as np
import shapely
from shapely.affinity import rotate, translate

from building_layout import WALL_M
from building_plan import FLOOR_M
from structural_shell_geometry import mesh_section, native
from terraced_mass_geometry import bands, physical, presentation
from terraced_mass_plan import plan
from terraced_mass_profiles import proposals
from terraced_mass_sources import sources


class TerracedMassTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = sources() + [
            {
                "id": "analytic",
                "properties": {"render_height": 18},
                "polygon": shapely.box(0, 0, 30, 20),
            },
            {
                "id": "concave",
                "properties": {"render_height": 15},
                "polygon": shapely.Polygon(
                    [(0, 0), (28, 0), (28, 12), (18, 12), (18, 24), (0, 24)]
                ),
            },
        ]
        cls.before = copy.deepcopy(cls.rows)
        cls.products = [
            (row, plan(row, kind))
            for row in cls.rows
            for kind in ("cascade", "paired-wings")
        ]
        cls.solids = [(row, p, physical(p)) for row, p in cls.products]

    def test_source_immutability_nested_profiles_holes_heights_and_stable_ids(self):
        for row, p in self.products:
            self.assertEqual(p.source_wkb, row["polygon"].wkb)
            self.assertEqual(p.storeys[0].footprint_wkb, p.source_wkb)
            self.assertEqual(p.storeys[0].bottom_m, 0)
            self.assertEqual(p.storeys[-1].top_m, row["properties"]["render_height"])
            self.assertEqual(len({s.identifier for s in p.storeys}), len(p.storeys))
            previous = row["polygon"]
            for i, storey in enumerate(p.storeys):
                current = shapely.from_wkb(storey.footprint_wkb)
                self.assertTrue(current.is_valid)
                self.assertEqual(current.geom_type, "Polygon")
                self.assertLessEqual(current.difference(previous).area, 1e-8)
                if i:
                    self.assertEqual(p.storeys[i - 1].top_m, storey.bottom_m)
                    terrace = shapely.from_wkb(storey.terrace_wkb)
                    self.assertLessEqual(
                        terrace.symmetric_difference(previous.difference(current)).area,
                        1e-8,
                    )
                for ring in row["polygon"].interiors:
                    self.assertLessEqual(
                        current.intersection(shapely.Polygon(ring)).area, 1e-8
                    )
                previous = current
            self.assertEqual(p, plan(row, p.kind))
            with self.assertRaises(FrozenInstanceError):
                p.kind = "changed"
        for before, after in zip(self.before, self.rows, strict=True):
            self.assertEqual(before["properties"], after["properties"])
            self.assertEqual(before["polygon"].wkb, after["polygon"].wkb)

    def test_rectangle_programme_areas_follow_independent_closed_forms(self):
        for row, p in self.products:
            if row["id"] != "analytic":
                continue
            first = max(1, len(p.storeys) // 3)
            actual = [
                shapely.from_wkb(s.footprint_wkb).area
                for s in (p.storeys[first], p.storeys[-1])
            ]
            axis, _side, distance = p.choice
            width, depth = (30, 20) if axis == 0 else (20, 30)
            if p.kind == "cascade":
                expected = [
                    (width - distance) * depth,
                    (width - 2 * distance) * (depth - distance),
                ]
            else:
                wing = distance
                expected = [
                    2 * width * wing + 6 * (depth - 2 * wing),
                    width * wing + 6 * (depth - wing),
                ]
            np.testing.assert_allclose(actual, expected, rtol=0, atol=1e-8)

    def test_reserved_stair_and_existing_cell_entry_are_contained_not_claimed_physical_access(
        self,
    ):
        for row, p in self.products:
            core = shapely.from_wkb(p.core.footprint_wkb)
            for storey in p.storeys:
                interior = shapely.from_wkb(storey.footprint_wkb).buffer(
                    -WALL_M, join_style="mitre"
                )
                self.assertTrue(interior.covers(core))
            self.assertTrue(
                core.buffer(1e-7).covers(shapely.from_wkb(p.core.opening_wkb))
            )
            path = np.asarray(p.core.path)
            self.assertTrue(core.buffer(1e-7).covers(shapely.LineString(path[:, :2])))
            self.assertTrue(np.all(np.diff(path[:, 2]) >= 0))
            self.assertEqual(path[0, 2], FLOOR_M)
            self.assertAlmostEqual(path[-1, 2], FLOOR_M + p.core.rise_m)
            self.assertLessEqual(p.core.riser_m, 0.18)
            self.assertAlmostEqual(p.core.risers * p.core.riser_m, p.core.rise_m)
            centre, tangent = np.asarray(p.entry.centre), np.asarray(p.entry.tangent)
            self.assertAlmostEqual(np.linalg.norm(tangent), 1.0)
            outward = np.array([tangent[1], -tangent[0]])
            for side in (-0.55, 0.55):
                point = centre + side * tangent
                self.assertLessEqual(
                    row["polygon"].exterior.distance(shapely.Point(point)), 1e-8
                )
                self.assertTrue(
                    row["polygon"].contains(shapely.Point(point - outward * 0.5))
                )
            self.assertGreater(
                shapely.from_wkb(p.entry.landing_wkb).difference(row["polygon"]).area,
                1.5,
            )

    def test_closed_union_volume_surface_sections_and_provenance(self):
        for row, p, product in self.solids:
            volumes = bands(p)
            solid = native(product.mesh, product.vertex_roots)
            expected_volume = sum(
                v.polygon.area * (v.high_m - v.low_m) for v in volumes
            )
            expected_area = 2 * row["polygon"].area + sum(
                v.polygon.length * (v.high_m - v.low_m) for v in volumes
            )
            self.assertAlmostEqual(solid.volume(), expected_volume, delta=1e-7)
            self.assertAlmostEqual(product.area_m2, expected_area, delta=1e-7)
            self.assertEqual(float(product.mesh["vertices"][:, 2].max()), p.top_m)
            self.assertEqual(float(product.mesh["vertices"][:, 2].min()), p.bottom_m)
            self.assertEqual(product.omissions, ())
            self.assertEqual(len(volumes), 3)
            self.assertEqual(set(product.source_ids), {v.identifier for v in volumes})
            self.assertEqual(set(product.source_indices), set(range(len(volumes))))
            for volume in volumes:
                region = mesh_section(
                    product.mesh,
                    product.vertex_roots,
                    (volume.low_m + volume.high_m) / 2,
                )
                self.assertLessEqual(
                    region.symmetric_difference(volume.polygon).area, 1e-7
                )

    def test_presentation_reuses_exact_surface_positions_and_metric_uv_normals(self):
        for _row, _p, product in self.solids:
            mesh = presentation(product)
            np.testing.assert_array_equal(
                mesh["vertices"],
                product.mesh["vertices"][product.mesh["faces"]].reshape(-1, 3),
            )
            np.testing.assert_allclose(
                np.linalg.norm(mesh["normals"], axis=1), 1, rtol=0, atol=1e-12
            )
            for xyz, uv in zip(
                mesh["vertices"][mesh["faces"]], mesh["uv"][mesh["faces"]], strict=True
            ):
                np.testing.assert_allclose(
                    np.linalg.norm(xyz - np.roll(xyz, 1, axis=0), axis=1),
                    np.linalg.norm(uv - np.roll(uv, 1, axis=0), axis=1),
                    rtol=0,
                    atol=1e-8,
                )

    def test_profile_geometry_is_rigid_transform_covariant(self):
        polygon = shapely.box(0, 0, 30, 20)
        for kind in ("cascade", "paired-wings"):
            expected = next(proposals(polygon, kind, 0))[0]
            moved = translate(rotate(polygon, 21, origin=(0, 0)), 1200, -730)
            actual = next(proposals(moved, kind, 0))[0]
            for a, b in zip(expected, actual, strict=True):
                transformed = translate(rotate(a, 21, origin=(0, 0)), 1200, -730)
                self.assertLessEqual(transformed.symmetric_difference(b).area, 1e-7)

    def test_invalid_or_inappropriate_sources_and_supplied_levels(self):
        row = self.rows[-2]
        for changed in (
            row | {"id": ""},
            row | {"clipped": True},
            row | {"properties": {"render_height": 5}},
            row | {"properties": {"render_height": float("nan")}},
            row | {"properties": {"render_height": 18, "render_min_height": 3}},
            row | {"properties": {"render_height": 18, "levels": 4.5}},
            row | {"properties": {"render_height": 18, "levels": 40}},
            row | {"polygon": shapely.box(0, 0, 2, 30)},
            row | {"polygon": shapely.Point(0, 0).buffer(20, quad_segs=40)},
            row | {"polygon": shapely.Polygon([(0, 0, 0), (30, 0, 0), (0, 20, 0)])},
        ):
            with self.assertRaises(ValueError):
                plan(changed, "cascade")
        with self.assertRaises(ValueError):
            plan(row, "unknown")
        supplied = plan(
            row | {"properties": row["properties"] | {"levels": 5}}, "cascade"
        )
        self.assertTrue(supplied.source_levels)
        self.assertEqual(len(supplied.storeys), 5)


if __name__ == "__main__":
    unittest.main()
