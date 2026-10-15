"""Actual source entrances retain cells, independent states and whole arrival clearance."""

import copy
import unittest

import numpy as np
import shapely
from demand import View
from entrance_clearance import clearance
from facade_entrances import EntranceDetails, demand
from facade_entry_geometry import products
from tile_facade_bands import BandDetails
from tile_facade_recess import attach
from verify_recess_consumer import camera, source_facades
from verify_urban_roofs import sources


def plan(row, facades, p, angle=0.0, neighbors=()):
    rows = [row, *neighbors]
    d = EntranceDetails(
        rows,
        shapely.STRtree([r["polygon"] for r in rows]),
        {row["id"] + ":floor-0:entrance": angle},
    )
    e = d.plan(row, p, facades, "residential")
    if e is None:
        raise AssertionError(d.rejected)
    return e, d


class FacadeEntrances(unittest.TestCase):
    def test_inside_arrival_keeps_floor_and_door_bundle_when_looking_away(self):
        row = sources()[1]
        _, f, p = source_facades(row)
        e, _ = plan(row, f, p)
        c, t = np.asarray(e["door"]["centre"]), np.asarray(e["door"]["tangent"])
        inward = np.array([-t[1], t[0]])
        eye = np.r_[c + inward * 1.2, 1.81]
        view = View(eye, eye + np.r_[inward, 0.0], width=1920, height=1080)
        self.assertEqual(demand(e, view), "through")

    def test_actual_sources_keep_ids_and_cells_across_open_closed_compact_far(self):
        for row in sources():
            old = copy.deepcopy(row)
            mesh, facades, p = source_facades(row)
            e, d = plan(row, facades, p)
            opened, other = plan(row, facades, p, 90.0)
            self.assertEqual(d.planned, other.planned)
            self.assertEqual(row, old)
            frame = e["frame"]
            view = camera(frame, 25)
            modes = []
            for distance in (25.0, 500.0, 100000.0):
                v = camera(frame, distance)
                out, records, rejected = attach(
                    mesh,
                    row,
                    p,
                    facades,
                    v,
                    lambda: 32769,
                    32768,
                    plain_wall_material=1,
                    entrance=e,
                )
                emitted = [r for r in records if r.get("kind") == "entrance"]
                modes.append(emitted[0]["mode"] if emitted else None)
                if distance == 100000.0:
                    self.assertIs(out, mesh)
                else:
                    self.assertEqual(len(emitted), 1, rejected)
                    self.assertEqual(emitted[0]["cell"], [e["column"], 0])
                    self.assertEqual(emitted[0]["id"], e["id"])
                    from blender_city_mesh import cell_sizes

                    self.assertTrue(np.isfinite(cell_sizes(out, (32768, 32769))).all())
            self.assertEqual(modes, ["through", "compact", None])
            self.assertEqual(demand(opened, view), "through")

    def test_real_open_actor_tube_and_closed_missing_landing_counterparts(self):
        row = sources()[1]
        _, f, p = source_facades(row)
        opened, _ = plan(row, f, p, 90.0)
        parts = products(opened, 1, "through")
        proof = clearance(opened, parts)
        self.assertGreater(proof["critical_prism_slabs"], 0)
        closed, _ = plan(row, f, p)
        with self.assertRaisesRegex(ValueError, "torso"):
            clearance(closed, products(closed, 1, "through"))
        with self.assertRaisesRegex(ValueError, "foot"):
            clearance(opened, [c for c in parts if not np.all(c["materials"] == 4)])

    def test_neighbor_landing_blocks_selected_cell_and_can_choose_other_facade(self):
        row = sources()[1]
        _, f, p = source_facades(row)
        entry, _ = plan(row, f, p)
        neighbor = {
            "id": "blocked",
            "polygon": entry["door"]["landing"],
            "properties": {"render_height": 10},
        }
        other, _ = plan(row, f, p, neighbors=(neighbor,))
        self.assertGreater(
            shapely.Point(entry["door"]["centre"]).distance(
                shapely.Point(other["door"]["centre"])
            ),
            0.1,
        )
        self.assertLess(
            other["door"]["landing"].intersection(neighbor["polygon"]).area, 1e-8
        )

    def test_plinth_does_not_close_the_entrance(self):
        row = sources()[1]
        mesh, f, p = source_facades(row)
        entry, _ = plan(row, f, p, 90.0)
        details = BandDetails([row])
        details(
            mesh,
            row,
            p,
            f,
            camera(entry["frame"], 25),
            1,
            "residential",
            entrance=entry,
        )
        index = f.index(entry["frame"])
        self.assertFalse(
            any(index in r["edges"] for r in details.completed if r["base_m"] < 2.31)
        )


if __name__ == "__main__":
    unittest.main()
