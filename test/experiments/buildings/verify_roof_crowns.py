"""Independent slices/integrals, real source programmes and immutable product selection."""

import copy
import math
import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from clad_facade_mesh import build as previous_build
from clad_facade_sources import sources as previous_sources
from provider_contract import attributes
from roof_crown_choice import prepare
from roof_crown_mesh import envelope
from roof_crown_plan import plan
from roof_crown_sources import sources
from structural_shell_geometry import mesh_section
from tile_roof_crowns import CrownFacades
from verify_facade_frames import view_for
from verify_roof_envelopes import measures


def expected_section(crown, height):
    p = shapely.from_wkb(crown.footprint_wkb)
    if height < crown.deck_m:
        return p
    if height >= crown.top_m:
        return shapely.Polygon()
    guard_fraction = (height - crown.deck_m) / crown.guard_m
    occupied = (
        p.difference(p.buffer(-(0.28 - 0.12 * guard_fraction), join_style="mitre"))
        if guard_fraction < 1
        else shapely.Polygon()
    )
    if crown.kind == "parapet-service":
        cap = shapely.from_wkb(
            next(r.footprint_wkb for r in crown.regions if r.name == "service-cap")
        )
        side = shapely.from_wkb(
            next(r.footprint_wkb for r in crown.regions if r.name == "service-side")
        )
        fraction = (height - crown.deck_m) / (crown.top_m - crown.deck_m)
        occupied = occupied.union(
            cap.union(side).buffer(-0.12 * fraction, join_style="mitre")
        )
    return occupied


def analytic(crown):
    p = shapely.from_wkb(crown.footprint_wkb)
    volume = p.area * crown.deck_m
    area = p.area + p.length * (crown.deck_m + crown.guard_m)
    for r in crown.regions:
        area += shapely.from_wkb(r.footprint_wkb).area * math.hypot(
            1, (r.high_m - r.low_m) / 0.12
        )
    nodes, weights = np.polynomial.legendre.leggauss(3)
    breaks = sorted({crown.deck_m, crown.deck_m + crown.guard_m, crown.top_m})
    for a, b in zip(breaks, breaks[1:]):
        volume += (
            (b - a)
            / 2
            * sum(
                w * expected_section(crown, (a + b) / 2 + x * (b - a) / 2).area
                for x, w in zip(nodes, weights, strict=True)
            )
        )
    return volume, area


class CrownTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sources = sources()
        cls.cases = []
        for row, _neighbors in cls.sources:
            for kind in ("parapet", "parapet-service"):
                p = plan(
                    row["polygon"],
                    attributes(row["properties"])["buildingHeight"],
                    kind,
                )
                cls.cases.append((row, p, envelope(p), envelope(p, coarse=True)))

    def test_real_hulls_directed_closure_volume_area_and_independent_height_slices(
        self,
    ):
        for row, p, full, _coarse in self.cases:
            solid = measures(full)
            volume, area = analytic(p)
            self.assertAlmostEqual(solid.volume(), volume, delta=1e-7)
            self.assertAlmostEqual(solid.surface_area(), area, delta=1e-7)
            heights = [p.deck_m * 0.5]
            heights.extend(
                p.deck_m + p.guard_m * t for t in (0.1, 0.25, 0.5, 0.75, 0.9)
            )
            if p.top_m > p.deck_m + p.guard_m:
                heights.append((p.deck_m + p.guard_m + p.top_m) / 2)
            for z in heights:
                actual = mesh_section(full.mesh, full.vertex_roots, z)
                self.assertLess(
                    actual.symmetric_difference(expected_section(p, z)).area, 1e-7
                )
            self.assertEqual(
                full.mesh["vertices"][:, 2].max(),
                attributes(row["properties"])["buildingHeight"],
            )
            self.assertLess(
                row["polygon"]
                .symmetric_difference(shapely.from_wkb(full.footprint_wkb))
                .area,
                1e-8,
            )

    def test_metric_uvs_geometric_unit_normals_and_coarse_material_coverage(self):
        for row, p, full, coarse in self.cases:
            coarse_solid = measures(coarse)
            self.assertAlmostEqual(
                coarse_solid.volume(), row["polygon"].area * p.top_m, delta=1e-7
            )
            self.assertLess(len(coarse.mesh["faces"]), len(full.mesh["faces"]))
            for env in (full, coarse):
                mesh = env.mesh
                xyz, uv = mesh["vertices"][mesh["faces"]], mesh["uv"][mesh["faces"]]
                np.testing.assert_allclose(
                    np.linalg.norm(mesh["normals"], axis=1), 1, atol=1e-12
                )
                normal = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
                normal /= np.linalg.norm(normal, axis=1, keepdims=True)
                np.testing.assert_allclose(
                    mesh["normals"][mesh["faces"]],
                    np.repeat(normal[:, None, :], 3, axis=1),
                    atol=1e-10,
                )
                for a, b in ((0, 1), (1, 2), (2, 0)):
                    np.testing.assert_allclose(
                        np.linalg.norm(xyz[:, a] - xyz[:, b], axis=1),
                        np.linalg.norm(uv[:, a] - uv[:, b], axis=1),
                        rtol=1e-9,
                        atol=1e-9,
                    )
                for material in (9, 10):
                    projected = shapely.union_all(
                        shapely.polygons(
                            xyz[
                                (env.surface_roles == 0)
                                & (mesh["materials"] == material),
                                :,
                                :2,
                            ]
                        )
                    )
                    expected = shapely.union_all(
                        [
                            shapely.from_wkb(r.footprint_wkb)
                            for r in p.regions
                            if r.material == material
                        ]
                    )
                    self.assertLess(projected.symmetric_difference(expected).area, 1e-8)

    def test_source_programmes_are_stable_and_supplied_fields_levels_and_rooms_retained(
        self,
    ):
        kinds = []
        for row, _neighbors in self.sources:
            before = copy.deepcopy(row)
            p, _reason = prepare(row)
            self.assertIsNotNone(p)
            kinds.append(p.kind)
            self.assertEqual(p, prepare(row)[0])
            self.assertEqual(row, before)
            for properties in (
                row["properties"] | {"roof:shape": "flat"},
                row["properties"] | {"building:levels": 100},
            ):
                changed = row | {"properties": properties}
                try:
                    candidate, _reason = prepare(changed)
                except ValueError:
                    candidate = None
                self.assertIsNone(candidate)
            small = row | {"polygon": shapely.box(0, 0, 20, 20)}
            self.assertIsNone(prepare(small)[0])
        self.assertEqual(set(kinds), {"parapet", "parapet-service"})

    def test_products_reused_without_generation_and_source_snapshot_identity_enforced(
        self,
    ):
        row, neighbors = self.sources[2]
        owner = CrownFacades([row, *neighbors], view_for(row))
        source = owner.rows[0]
        roof = owner.base
        asset = roof.assets[row["id"]]
        parameters = attributes(source["properties"])
        first, info = roof(source, parameters, 12)
        self.assertEqual(info["roof_crown_product"], "full")
        far = replace(
            roof.view, eye=roof.view.target + (roof.view.eye - roof.view.target) * 10000
        )
        roof.view = far
        with (
            patch("tile_roof_crowns.prepare", side_effect=AssertionError("replanned")),
            patch("tile_roof_crowns.envelope", side_effect=AssertionError("rebuilt")),
        ):
            second, other = roof(source, parameters, 13)
        self.assertEqual(other["roof_crown_product"], "coarse")
        self.assertEqual(info["roof_crown_plan"], other["roof_crown_plan"])
        self.assertIs(first["vertices"], asset.full.mesh["vertices"])
        self.assertIs(second["vertices"], asset.coarse.mesh["vertices"])
        for env in (asset.full, asset.coarse):
            self.assertTrue(all(not a.flags.writeable for a in env.mesh.values()))
            self.assertFalse(env.vertex_roots.flags.writeable)
        with self.assertRaises(ValueError):
            roof(dict(source), parameters, 12)

    def test_actual_facades_use_reserved_roof_datum_with_existing_ground_access(self):
        for row, neighbors in self.sources:
            owner = CrownFacades([row, *neighbors], view_for(row))
            source = owner.rows[0]
            mesh, info = owner(source, attributes(source["properties"]), 12)
            self.assertEqual(info["roof_crown_product"], "full")
            self.assertFalse(owner.rejected)
            self.assertTrue(info["facade_plan"])
            self.assertTrue(info["entrance_plan"])
            for facade in info["facade_plan"]:
                self.assertLessEqual(facade["height_m"], info["eave_m"])
            p = owner.base.assets[source["id"]].plan
            if p.kind == "parapet-service":
                cap = shapely.from_wkb(
                    next(r.footprint_wkb for r in p.regions if r.name == "service-cap")
                )
                side = shapely.from_wkb(
                    next(r.footprint_wkb for r in p.regions if r.name == "service-side")
                )
                self.assertTrue(
                    source["polygon"]
                    .buffer(-1.49, join_style="mitre")
                    .covers(cap.union(side))
                )
            self.assertEqual(
                mesh["vertices"][:, 2].max(),
                attributes(source["properties"])["buildingHeight"],
            )

    def test_existing_nonflat_source_produces_exact_previous_facade_and_function_fields(
        self,
    ):
        row, neighbors = previous_sources()[1]
        view = view_for(row)
        before, old, _owner = previous_build(row, neighbors, view)
        current = CrownFacades([row, *neighbors], view)
        after, new = current(current.rows[0], attributes(row["properties"]), 12)
        self.assertEqual(set(before), set(after))
        for key in before:
            np.testing.assert_array_equal(before[key], after[key])
        for key in old:
            if not key.endswith("_ms"):
                self.assertEqual(old[key], new[key], key)

    def test_invalid_low_narrow_degenerate_and_unsupplied_service_fallback(self):
        bad = [
            shapely.Polygon(),
            shapely.Polygon([(0, 0), (2, 2), (2, 0), (0, 2)]),
            shapely.box(0, 0, 100, 0.3),
        ]
        for p in bad:
            with self.assertRaises(ValueError):
                plan(p, 8, "parapet")
        for top in (float("nan"), 3.0):
            with self.assertRaises(ValueError):
                plan(shapely.box(0, 0, 40, 30), top, "parapet")
        narrow = shapely.Polygon(
            [(0, 0), (60, 0), (60, 40), (0, 40)],
            holes=[[(12, 8), (12, 30), (48, 30), (48, 8)]],
        )
        p = plan(narrow, 18, "parapet-service")
        self.assertEqual(p.kind, "parapet")
        self.assertIn("no contained", p.service_reason)
        measures(envelope(p))


if __name__ == "__main__":
    unittest.main()
