"""Actual source/window-bay consumers and independent physical/retained-surface negatives."""

import copy
import json
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from demand import View
from entrance_clearance import clearance
from facade_entrances import EntranceDetails
from facade_entry_geometry import basis_for, expand
from facade_window_bays import WindowBayDetails, demand
from outshine_ref.buildings.interiors.geometry import rectangle
from outshine_ref.buildings.modelling.prism_union import PrismVolume, primitive_mesh
from provider_contract import seed_for
from tile_buildings import construct
from tile_facade_recess import attach
from tile_facades import FacadeDetails
from verify_recess_consumer import source_facades
from window_bay_geometry import products


def sources():
    saved = json.loads(
        Path(__file__).with_name("data").joinpath("window-bays.json").read_text()
    )
    return [
        r
        | {
            "polygon": shapely.from_wkb(r["footprint_wkb"]),
            "clipped": False,
            "layer": "building",
        }
        for r in saved["cases"]
    ]


def prepared(row, neighbors=()):
    mesh, frames, parameters = source_facades(row)
    rows = [row, *neighbors]
    entrances = EntranceDetails(rows, shapely.STRtree([r["polygon"] for r in rows]))
    entry = entrances.plan(row, parameters, frames, "residential")
    if entry is None:
        raise AssertionError(entrances.rejected)
    owner = WindowBayDetails(entrances)
    fraction = (1.0, 0.65)[seed_for(row["id"]) % 3]
    plan = owner.plan(row, entry, "residential", fraction)
    return mesh, frames, parameters, entry, owner, plan


def camera(current, distance):
    basis = basis_for(current["frame"])
    target = np.r_[current["centre"], current["floor_z_m"] + 1.6]
    return View(
        target + (basis[2] + basis[0] * 0.3) * distance,
        target,
        width=1920,
        height=1080,
        fov_deg=48,
    )


def inspect(current, parts, **kwargs):
    entry = {"door": {"centre": current["centre"], "tangent": current["tangent"]}}
    return clearance(
        entry, parts, floor_z_m=current["floor_z_m"], outside_m=0.35, **kwargs
    )


class WindowBayCells(unittest.TestCase):
    def test_actual_plan_retains_sources_ids_and_defers_all_mesh_templates(self):
        for row in sources():
            before = copy.deepcopy(row)
            _, _, _, entry, owner, plan = prepared(row)
            self.assertIsNotNone(plan, owner.rejected)
            self.assertFalse(owner.templates)
            self.assertEqual(row, before)
            saved = copy.deepcopy(owner.planned[-1])
            other = owner.plan(row, entry, "residential", saved["front_fraction"])
            self.assertEqual(owner.planned[-1], saved)
            self.assertEqual(
                [c["id"] for c in plan["cells"]], [c["id"] for c in other["cells"]]
            )
            for c in plan["cells"]:
                self.assertEqual(
                    c["height_interval_m"],
                    (c["base_m"], c["base_m"] + entry["floor_m"]),
                )
                self.assertLess(c["footprint"].intersection(row["polygon"]).area, 1e-8)

    def test_actual_supported_passage_and_closed_back_missing_floor_wrong_floor_negatives(
        self,
    ):
        for row in sources():
            _, _, _, _, owner, plan = prepared(row)
            for c in plan["cells"]:
                actual = products(c, owner, 1, "through")
                self.assertEqual(inspect(c, actual)["actor_radius_m"], 0.3)
            c = plan["cells"][0]
            actual = products(c, owner, 1, "through")
            wall = rectangle(c["centre"], c["tangent"], c["cell_width_m"], 0, 0.24)
            v, f = primitive_mesh(
                PrismVolume("closed-back", wall, *c["height_interval_m"])
            )
            closed = expand(
                {
                    "vertices": v,
                    "faces": f,
                    "materials": np.ones(len(f), dtype=np.uint8),
                },
                1,
            )
            for bad in ([closed, *actual[1:]], [actual[0], *actual[2:]]):
                with self.assertRaises(ValueError):
                    inspect(c, bad)
            with self.assertRaises(ValueError):
                inspect(c | {"floor_z_m": c["floor_z_m"] + 0.1}, actual)
            shallow = c | {"prototype": replace(c["prototype"], depth_m=0.45)}
            with self.assertRaises(ValueError):
                inspect(c, products(shallow, owner, 1, "through"))

    def test_actual_other_cells_and_ground_floor_keep_surface_material_and_metric_uvs(
        self,
    ):
        from verify_shopfronts import Shopfronts

        for row in sources():
            mesh, frames, p, entry, _, plan = prepared(row)
            for distance, expected in (
                (10, "through"),
                (500, "compact"),
                (100000, None),
            ):
                view = camera(plan["cells"][0], distance)
                kwargs = {"plain_wall_material": 1, "entrance": entry}
                before, _, _ = attach(
                    mesh, row, p, frames, view, lambda: 32769, 32768, **kwargs
                )
                after, records, _ = attach(
                    mesh,
                    row,
                    p,
                    frames,
                    view,
                    lambda: 32769,
                    32768,
                    bays=plan,
                    **kwargs,
                )
                emitted = [r for r in records if r.get("kind") == "window_bay"]
                self.assertEqual(
                    {r["mode"] for r in emitted}, {expected} if expected else set()
                )
                excluded = {plan["frame"]["id"]: {tuple(r["cell"]) for r in emitted}}
                Shopfronts.assert_upper_surface(
                    self,
                    before,
                    after,
                    frames,
                    entry["floor_m"],
                    excluded_cells=excluded,
                )
                Shopfronts.assert_upper_surface(
                    self, before, after, frames, 0, ceiling=entry["floor_m"]
                )

    def test_full_height_neighbor_is_not_mistaken_for_clear_balcony_guard_height(self):
        row = sources()[0]
        _, _, _, _, _, plan = prepared(row)
        c = plan["cells"][0]
        neighbor = {
            "id": "actual-context-negative",
            "layer": "building",
            "polygon": c["footprint"].buffer(0.02),
            "properties": {
                "render_min_height": c["base_m"] + 2.0,
                "render_height": c["base_m"] + 2.5,
            },
        }
        _, _, _, _, owner, after = prepared(row, (neighbor,))
        self.assertNotIn(c["id"], [r["id"] for r in after["cells"]] if after else [])
        self.assertTrue(
            any(
                r["cell"] == [c["column"], c["row"]]
                and "height interval" in r["reason"]
                for r in owner.rejected
            )
        )

    def test_inside_lookaway_keeps_actual_hull_and_capacity_falls_back_atomically(self):
        row = sources()[0]
        mesh, frames, p, entry, _, plan = prepared(row)
        c = plan["cells"][0]
        eye = np.r_[c["pocket"].centroid.coords[0], c["floor_z_m"] + 1.6]
        lookaway = View(eye, eye + [1, 0, 0], width=1920, height=1080)
        self.assertEqual(demand(c, lookaway), "through")
        _, records, rejected = attach(
            mesh,
            row,
            p,
            frames,
            camera(c, 10),
            lambda: 32769,
            32768,
            plain_wall_material=1,
            entrance=entry,
            bays=plan,
            max_cells=0,
        )
        self.assertFalse(any(r.get("kind") == "window_bay" for r in records))
        self.assertTrue(any("capacity" in r["reason"] for r in rejected))

    def test_actual_default_consumer_uses_both_shapes_exclusively_and_retains_source_grid_entry(
        self,
    ):
        for row in sources():
            _, _, _, _, _, plan = prepared(row)
            view = camera(plan["cells"][0], 10)
            after = FacadeDetails([row], view)
            mesh_after, source_after = construct(
                "actual-bay",
                prepared=([row], {"invalid": [], "span_m": 100.0}),
                ordinary_builder=after,
            )
            before = FacadeDetails([row], view)
            with patch.object(before.bays, "plan", return_value=None):
                _, source_before = construct(
                    "actual-bay",
                    prepared=([row], {"invalid": [], "span_m": 100.0}),
                    ordinary_builder=before,
                )
            changed_programme = {"balcony_plan", "window_bay_plan", "facade_programme"}
            for a, b in zip(
                source_after["members"], source_before["members"], strict=True
            ):
                self.assertEqual(
                    {k: v for k, v in a.items() if k != "prototype"},
                    {k: v for k, v in b.items() if k != "prototype"},
                )
                self.assertEqual(
                    {
                        k: v
                        for k, v in a["prototype"].items()
                        if k not in changed_programme
                    },
                    {
                        k: v
                        for k, v in b["prototype"].items()
                        if k not in changed_programme
                    },
                )
            self.assertFalse(after.balconies.planned)
            self.assertTrue(after.bay_geometry, after.recess_rejected)
            self.assertFalse(np.any(mesh_after["materials"] == 3))
            self.assertEqual(after.entrances.planned, before.entrances.planned)
            self.assertEqual(after.shopfronts.planned, before.shopfronts.planned)
            self.assertEqual(
                after.completed[0]["facades"], before.completed[0]["facades"]
            )


if __name__ == "__main__":
    unittest.main()
