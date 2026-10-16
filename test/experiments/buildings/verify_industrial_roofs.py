"""Real provider halls, stable logical roof choices and explicit inference limits."""

import copy
import json
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from industrial_roof_choice import KINDS, metadata
from outshine_ref.buildings.modelling import heightfield_envelope
from provider_contract import attributes
from stepped_source_probe import overview
from tile_facades import FacadeDetails
from tile_roof_envelopes import OrdinaryDetails, choose, roof_surface
from verify_roof_envelopes import measures


def sources():
    saved = json.loads(
        Path(__file__).with_name("data").joinpath("industrial-roofs.json").read_text()
    )

    def restore(row):
        return row | {"polygon": shapely.from_wkb(row["footprint_wkb"])}

    return [
        (restore(c["source"]), [restore(r) for r in c["neighbors"]])
        for c in saved["cases"]
    ]


def view_for(row):
    a, b, c, d = row["polygon"].bounds
    p = attributes(row["properties"])
    return overview(
        np.array(
            [
                [x, y, z]
                for x in (a, c)
                for y in (b, d)
                for z in (0, p["buildingHeight"])
            ]
        ),
        [1, -1, 0.7],
    )


def upper_wall_regions(mesh, row, eave):
    points = mesh["vertices"][mesh["faces"]]
    directions = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    directions /= np.linalg.norm(directions, axis=1, keepdims=True)
    regions = []
    for ring in (
        shapely.orient_polygons(row["polygon"]).exterior,
        *shapely.orient_polygons(row["polygon"]).interiors,
    ):
        xy = np.asarray(ring.coords)
        for a, b in zip(xy[:-1], xy[1:], strict=True):
            tangent = (b - a) / np.linalg.norm(b - a)
            normal = np.array([tangent[1], -tangent[0]])
            selected = (abs((points[:, :, :2] - a) @ normal).max(axis=1) < 1e-7) & (
                directions[:, :2] @ normal > 0.999999
            )
            projected = np.stack(
                ((points[selected, :, :2] - a) @ tangent, points[selected, :, 2]),
                axis=2,
            )
            regions.append(
                shapely.union_all(shapely.polygons(projected)).intersection(
                    shapely.box(0, eave, np.linalg.norm(b - a), 100)
                )
            )
    return regions


class IndustrialRoofs(unittest.TestCase):
    def test_real_source_volume_bounds_closure_and_unchanged_inputs(self):
        kinds = set()
        for row, _ in sources():
            before = copy.deepcopy(row)
            p = attributes(row["properties"])
            choice = choose(row, p)
            kinds.add(choice.kind)
            top = roof_surface(row, choice)[0]
            envelope = heightfield_envelope(top, 0)
            measures(envelope)
            self.assertGreater(envelope.volume_m3, row["polygon"].area * choice.eave_m)
            self.assertLess(
                envelope.volume_m3, row["polygon"].area * p["buildingHeight"]
            )
            self.assertLess(
                shapely.from_wkb(envelope.footprint_wkb)
                .symmetric_difference(row["polygon"])
                .area,
                1e-7,
            )
            self.assertAlmostEqual(top["vertices"][:, 2].max(), p["buildingHeight"])
            self.assertGreaterEqual(choice.eave_m, 3.15)
            self.assertEqual(row, before)
            self.assertIn("unproven", choice.reason)
        self.assertEqual(kinds, set(KINDS))

    def test_far_choice_stays_logical_without_roof_clipping(self):
        for row, neighbors in sources():
            p, view = attributes(row["properties"]), view_for(row)
            far = replace(view, eye=view.target + (view.eye - view.target) * 1000)
            builder = OrdinaryDetails([row, *neighbors], far)
            with patch(
                "tile_roof_envelopes.roof_surface",
                side_effect=AssertionError("unneeded roof"),
            ):
                mesh, info = builder(row, p, 1)
            self.assertEqual(info["roof_section"], metadata(row, choose(row, p)))
            self.assertLessEqual(info["projected_geometry_error_px"], 0.5)
            self.assertTrue(len(mesh["faces"]))

    def test_suppressed_supplied_narrow_high_and_special_sources_keep_prior_choice(
        self,
    ):
        base = sources()[0][0]
        variants = [base | {"clipped": True}]
        for extra in (
            {"roof:shape": "flat"},
            {"roof:shape": "hipped"},
            {"building": "water_tower"},
            {"render_height": 80},
            {"render_height": 3},
            {"building:levels": 3},
            {"hide_3d": True},
        ):
            variants.append(base | {"properties": base["properties"] | extra})
        variants.append(base | {"polygon": shapely.box(0, 0, 3, 200)})
        for row in variants:
            self.assertNotIn(choose(row, attributes(row["properties"])).kind, KINDS)

    def test_actual_consumer_replans_facades_below_same_height_section(self):
        for row, neighbors in sources():
            p = attributes(row["properties"])
            owner = FacadeDetails([row, *neighbors], view_for(row))
            mesh, info = owner(row, p, 1)
            self.assertIn(info["roof_kind"], KINDS)
            self.assertEqual(info["roof_section"], metadata(row, choose(row, p)))
            self.assertTrue(info["facade_plan"])
            for facade in info["facade_plan"]:
                self.assertAlmostEqual(facade["height_m"], info["eave_m"])
            self.assertAlmostEqual(mesh["vertices"][:, 2].max(), p["buildingHeight"])
            self.assertTrue(info.get("entrance_plan"), list(info))
            self.assertFalse(owner.rejected)
            original = heightfield_envelope(roof_surface(row, choose(row, p))[0], 0)
            for old, new in zip(
                upper_wall_regions(original.mesh, row, info["eave_m"]),
                upper_wall_regions(mesh, row, info["eave_m"]),
                strict=True,
            ):
                self.assertLess(old.symmetric_difference(new).area, 1e-7)
            roof = mesh["materials"] == 9
            expected = roof_surface(row, choose(row, p))[0]
            self.assertEqual(roof.sum(), len(expected["faces"]))
            np.testing.assert_array_equal(
                mesh["vertices"][mesh["faces"][roof]],
                expected["vertices"][expected["faces"]],
            )

    def test_local_surface_failure_keeps_whole_body_with_reason(self):
        row, neighbors = sources()[0]
        p = attributes(row["properties"])
        owner = FacadeDetails([row, *neighbors], view_for(row))
        with patch(
            "tile_roof_envelopes.roof_surface",
            side_effect=ValueError("section counterexample"),
        ):
            mesh, info = owner(row, p, 1)
        self.assertTrue(len(mesh["faces"]))
        self.assertTrue(
            any("section counterexample" in r["reason"] for r in owner.base.rejected)
        )
        self.assertNotIn("roof_section", info)


if __name__ == "__main__":
    unittest.main()
