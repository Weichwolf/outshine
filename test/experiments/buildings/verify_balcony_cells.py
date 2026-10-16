"""Actual residential cell attachments, state identity, whole local support and obstruction negatives."""

import copy
import unittest

import numpy as np
import shapely
from balcony_clearance import inspect
from balcony_geometry import products
from demand import View
from facade_balconies import BalconyDetails, demand
from facade_entrances import EntranceDetails
from facade_entry_geometry import basis_for, expand
from outshine_ref.buildings.balconies import volumes
from outshine_ref.buildings.interiors.geometry import rectangle
from outshine_ref.buildings.modelling import union_prisms
from tile_facade_recess import attach
from verify_recess_consumer import source_facades
from verify_urban_roofs import sources


def prepared(row, neighbors=(), states=None):
    mesh, frames, parameters = source_facades(row)
    rows = [row, *neighbors]
    entrances = EntranceDetails(rows, shapely.STRtree([r["polygon"] for r in rows]))
    entry = entrances.plan(row, parameters, frames, "residential")
    if entry is None:
        raise AssertionError(entrances.rejected)
    owner = BalconyDetails(entrances, states)
    plan = owner.plan(row, entry, "residential")
    return mesh, frames, parameters, entry, owner, plan


def camera(current, distance):
    door = current["doorway"]["door"]
    basis = basis_for(current["doorway"]["frame"])
    target = np.r_[door["centre"], current["floor_z_m"] + 1.6]
    return View(
        target + (basis[2] + basis[0] * 0.3) * distance,
        target,
        width=1920,
        height=1080,
        fov_deg=48,
    )


class BalconyCells(unittest.TestCase):
    def test_actual_plans_keep_source_ids_and_do_not_expand_templates_until_demand(
        self,
    ):
        for row in sources()[:2]:
            before = copy.deepcopy(row)
            _, _, _, entry, owner, plan = prepared(row)
            self.assertIsNotNone(plan, owner.rejected)
            self.assertLessEqual(len(plan["cells"]), 6)
            self.assertEqual(owner.templates, {})
            states = {c["door_id"]: 90.0 for c in plan["cells"]}
            _, _, _, _, opened, other = prepared(row, states=states)
            self.assertEqual(owner.planned, opened.planned)
            self.assertEqual(
                {c["id"] for c in other["cells"]}, {c["id"] for c in plan["cells"]}
            )
            for c in plan["cells"]:
                self.assertGreater(c["row"], 0)
                self.assertLessEqual(
                    c["floor_z_m"] + 2.15, c["base_m"] + entry["floor_m"] - 0.18
                )
                self.assertLess(c["footprint"].intersection(row["polygon"]).area, 1e-8)
            self.assertEqual(row, before)

    def test_supported_open_arrival_and_guard_with_closed_missing_deck_and_wrong_datum_negatives(
        self,
    ):
        for row in sources()[:2]:
            _, _, _, _, owner, plan = prepared(row)
            current = plan["cells"][0]
            closed = products(current, owner, 1, "through")
            with self.assertRaisesRegex(ValueError, "torso"):
                inspect(current, closed)
            current["doorway"]["angle_deg"] = 90.0
            opened = products(current, owner, 1, "through")
            proof = inspect(current, opened)
            self.assertGreater(proof["guard_intervals"], 0)
            with self.assertRaises(ValueError):
                inspect(current, opened[1:])
            with self.assertRaisesRegex(ValueError, "support"):
                inspect(current | {"floor_z_m": current["floor_z_m"] + 0.12}, opened)
            deck = union_prisms([volumes(current["prototype"])[0]])
            basis = basis_for(current["doorway"]["frame"])
            transform = np.array([basis[0], -basis[2], basis[1]])
            raw = {
                "vertices": deck.mesh["vertices"] @ transform
                + np.r_[current["doorway"]["door"]["centre"], current["floor_z_m"]],
                "faces": deck.mesh["faces"],
                "materials": np.full(len(deck.mesh["faces"]), 4, dtype=np.uint8),
            }
            with self.assertRaisesRegex(ValueError, "guard"):
                inspect(current, [expand(raw, 1), *opened[1:]])
            self.assertEqual(len(owner.templates), 1)

    def test_existing_cells_modes_and_capacity_fallback(self):
        from blender_city_mesh import cell_sizes

        for row in sources()[:2]:
            mesh, frames, parameters, entry, _owner, plan = prepared(row)
            current = plan["cells"][0]
            for distance, mode in ((10, "through"), (500, "compact"), (100000, None)):
                view = camera(current, distance)
                self.assertEqual(demand(current, view), mode)
                result, records, rejected = attach(
                    mesh,
                    row,
                    parameters,
                    frames,
                    view,
                    lambda: 32769,
                    32768,
                    plain_wall_material=1,
                    entrance=entry,
                    balconies=plan,
                )
                emitted = [r for r in records if r.get("kind") == "balcony"]
                if mode:
                    self.assertIn(current["id"], {r["id"] for r in emitted}, rejected)
                    self.assertEqual(
                        result["vertices"][:, 2].max(), mesh["vertices"][:, 2].max()
                    )
                    self.assertTrue(
                        np.isfinite(cell_sizes(result, (32768, 32769))).all()
                    )
                else:
                    self.assertFalse(emitted)
                    self.assertIs(result, mesh)
            _, records, rejected = attach(
                mesh,
                row,
                parameters,
                frames,
                camera(current, 10),
                lambda: 32769,
                32768,
                plain_wall_material=1,
                entrance=entry,
                balconies=plan,
                max_cells=0,
            )
            self.assertFalse(any(r.get("kind") == "balcony" for r in records))
            self.assertTrue(any("capacity" in r["reason"] for r in rejected))

    def test_neighbor_height_and_opposing_deck_margin(self):
        row = sources()[0]
        _, _, _, entry, _, plan = prepared(row)
        frame = entry["frame"]
        tangent = np.asarray(entry["door"]["tangent"])
        centre = (np.asarray(frame["a"]) + frame["b"]) / 2
        outside = rectangle(centre, tangent, frame["width_m"] + 2, 2.2, -2.0)
        neighbor = {
            "id": "opposing-deck-negative",
            "polygon": outside,
            "properties": {"render_height": 20, "render_min_height": 0},
        }
        self.assertIsNone(prepared(row, [neighbor])[-1])
        high = neighbor | {"properties": {"render_height": 30, "render_min_height": 20}}
        self.assertEqual(
            prepared(row, [high])[-2].planned[0]["cells"],
            prepared(row)[-2].planned[0]["cells"],
        )
        current = plan["cells"][0]
        eye = np.r_[
            current["pocket"].representative_point().coords[0],
            current["floor_z_m"] + 1.6,
        ]
        inward = -basis_for(frame)[2]
        self.assertEqual(demand(current, View(eye, eye + inward)), "through")

    def test_nonresidential_and_known_source_wing_keep_existing_skin(self):
        row = sources()[0]
        _, _, _, entry, owner, _plan = prepared(row)
        for family in ("commercial", "mixed-use", "industrial", "unknown"):
            self.assertIsNone(owner.plan(row, entry, family))
        frame = entry["frame"]
        centre = (np.asarray(frame["a"]) + frame["b"]) / 2
        wing = rectangle(centre, entry["door"]["tangent"], frame["width_m"], 1.2, 0.0)
        changed = row | {"polygon": row["polygon"].union(wing)}
        self.assertIsNone(owner.plan(changed, entry, "residential"))
        self.assertTrue(any("another wing" in r["reason"] for r in owner.rejected))
