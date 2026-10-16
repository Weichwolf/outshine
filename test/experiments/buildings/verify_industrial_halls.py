"""Actual loading/personnel passages and supported internal ramps on provider halls."""

import copy
import json
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import manifold3d as manifold
import numpy as np
import shapely
from industrial_hall_details import products
from industrial_hall_plan import plan
from industrial_hall_shell import structure
from provider_contract import attributes
from structural_shell_geometry import native
from tile_facades import FacadeDetails
from tile_roof_envelopes import choose, roof_surface
from verify_industrial_roofs import sources as roof_sources
from verify_industrial_roofs import view_for


def sources():
    saved = json.loads(
        Path(__file__).with_name("data").joinpath("industrial-halls.json").read_text()
    )

    def restore(row):
        return row | {"polygon": shapely.from_wkb(row["footprint_wkb"])}

    return [
        (restore(c["source"]), [restore(r) for r in c["neighbors"]])
        for c in saved["cases"]
    ]


def construct(row, neighbors=(), *, opened=1, staff_angle=90, view=None):
    view = view or view_for(row)
    owner = FacadeDetails(
        [row, *neighbors],
        view,
        gate_states={row["id"] + ":floor-0:loading-0": opened},
        door_states={row["id"] + ":floor-0:entrance": staff_angle},
    )
    mesh, info = owner(row, attributes(row["properties"]), 1)
    if info.get("hall_render") != "constructed":
        raise ValueError(str(owner.halls.rejected))
    value = plan(
        row,
        attributes(row["properties"]),
        info,
        info["facade_plan"],
        owner.entrances.plan(
            row, attributes(row["properties"]), info["facade_plan"], "industrial"
        ),
        owner.entrances.clear,
    )
    body, shell = structure(value, owner.base.current_envelope, 1)
    chunks, _ = products(value, 1, opened, view)
    solids = [native(body, shell.vertex_roots), *[native(c) for c in chunks]]
    collision = manifold.Manifold.batch_boolean(solids, manifold.OpType.Add)
    if collision.status() != manifold.Error.NoError:
        raise ValueError("Hall assembly rejected")
    return owner, mesh, info, value, shell, chunks, collision


def loading_access(value, collision, opened):
    gate = value["gate"]
    t = np.r_[gate["tangent"], 0]
    inward = np.array([-t[1], t[0], 0])
    centre = np.r_[gate["centre"], 0]
    for u in (-1, 0, 1):
        for z in (1.0, 1.9, 2.8):
            a = centre + t * u - inward * 0.20 + [0, 0, z]
            b = centre + t * u + inward * 6 + [0, 0, z]
            hits = collision.ray_cast(a, b)
            expected = gate["height_m"] * opened < z
            if bool(hits) != expected:
                raise ValueError(f"Loading state/path mismatch {opened=} {u=} {z=}")
    for depth in np.linspace(0.08, 5, 33):
        floor = 0.16 * min(depth / 3, 1)
        for u in (-1, 0, 1):
            xy = centre + t * u + inward * depth
            hits = collision.ray_cast(xy + [0, 0, floor + 0.5], xy + [0, 0, -0.1])
            if not hits or abs(hits[0].position[2] - floor) > 1e-7:
                raise ValueError(
                    "Internal ramp has an unsupported step or incorrect floor"
                )
            if opened == 1 and collision.ray_cast(
                xy + [0, 0, floor + 0.02], xy + [0, 0, floor + 2.6]
            ):
                raise ValueError("Loading route lacks actual 2.6m headroom")


def personnel_access(value, collision, angle):
    entry = value["entry"]
    t = np.asarray(entry["door"]["tangent"])
    inside = np.r_[[-t[1], t[0]], 0]
    centre = np.r_[entry["door"]["centre"], 1.1]
    hits = collision.ray_cast(centre - inside * 0.4, centre + inside * 2)
    if bool(hits) != (angle == 0):
        raise ValueError("Personnel opening does not match actual hinged leaf")


def hall_route(value, collision):
    entry, gate = value["entry"]["door"], value["gate"]
    t = np.asarray(gate["tangent"])
    inward = np.array([-t[1], t[0]])
    first = np.asarray(entry["centre"]) + inward * 1.5
    last = np.asarray(gate["centre"]) + inward * 3.8
    corner = np.asarray(entry["centre"]) + inward * 3.8
    centres = [
        a * (1 - f) + b * f
        for a, b in ((first, corner), (corner, last))
        for f in np.linspace(0, 1, 17)
    ]
    for centre in centres:
        for angle in np.linspace(0, 2 * np.pi, 8, endpoint=False):
            xy = centre + 0.29 * np.array([np.cos(angle), np.sin(angle)])
            if collision.ray_cast([*xy, 0.18], [*xy, 2.06]):
                raise ValueError("Actual personnel/loading path lacks pawn clearance")
            hits = collision.ray_cast([*xy, 0.5], [*xy, -0.1])
            if not hits or abs(hits[0].position[2] - 0.16) > 1e-7:
                raise ValueError("Personnel/loading route lacks continuous hall floor")


def roof_preserved(mesh, row):
    original = roof_surface(row, choose(row, attributes(row["properties"])))[0]
    actual = mesh["vertices"][mesh["faces"][mesh["materials"] == 9]]
    expected = original["vertices"][original["faces"]]
    if len(actual) != len(expected):
        raise ValueError("Hall CSG changed the number of source roof triangles")
    for triangle in expected:
        error = np.linalg.norm(actual[:, :, None] - triangle[None, None], axis=3)
        match = (error.min(axis=1).max(axis=1) < 1e-8) & (
            error.min(axis=2).max(axis=1) < 1e-8
        )
        if match.sum() != 1:
            raise ValueError("Hall CSG changed a source roof triangle")


class IndustrialHalls(unittest.TestCase):
    def test_real_halls_all_gate_states_actual_free_ramp_and_personnel_paths(self):
        for row, neighbors in sources()[:3]:
            before = copy.deepcopy(row)
            canonical = None
            for opened in (0, 0.5, 1):
                owner, mesh, info, value, _shell, _chunks, collision = construct(
                    row, neighbors, opened=opened
                )
                loading_access(value, collision, opened)
                personnel_access(value, collision, 90)
                hall_route(value, collision)
                roof_preserved(mesh, row)
                self.assertEqual(info["hall_plan"], canonical or info["hall_plan"])
                canonical = info["hall_plan"]
                self.assertEqual(info["room_ids"], [[value["room_id"]]])
                self.assertLessEqual(len(value["windows"]), 96)
                self.assertGreater(len(value["windows"]), 10)
                self.assertAlmostEqual(
                    mesh["vertices"][:, 2].max(),
                    attributes(row["properties"])["buildingHeight"],
                )
                self.assertGreater(collision.volume(), 0)
                self.assertFalse(owner.halls.rejected)
            self.assertEqual(row, before)

    def test_personnel_closed_state_really_blocks_the_same_opening(self):
        row, neighbors = sources()[0]
        *_, value, _shell, _chunks, collision = construct(row, neighbors, staff_angle=0)
        personnel_access(value, collision, 0)

    def test_actual_source_without_loading_cell_keeps_previous_facade_fallback(self):
        row, neighbors = sources()[3]
        owner = FacadeDetails([row, *neighbors], view_for(row))
        mesh, info = owner(row, attributes(row["properties"]), 1)
        self.assertNotEqual(info.get("hall_render"), "constructed")
        self.assertIn("known-building clearance", owner.halls.rejected[0]["reason"])
        self.assertGreater(len(mesh["faces"]), 0)

    def test_known_neighbor_blocks_all_cargo_candidates_before_expansion(self):
        row, _ = sources()[0]
        owner, _, info, value, *_ = construct(row)
        frame = value["entry"]["frame"]
        from outshine_ref.buildings.interiors.geometry import rectangle

        centre = (np.asarray(frame["a"]) + frame["b"]) / 2
        blocker = row | {
            "id": "explicit-neighbor-negative",
            "polygon": rectangle(
                centre, value["gate"]["tangent"], frame["width_m"] + 1, 4.1, -0.01
            ),
        }
        other = FacadeDetails([row, blocker], view_for(row))
        # Keep the actual admitted personnel plan to isolate loading-site clearance.
        with patch(
            "tile_industrial_halls.structure", side_effect=AssertionError("blocked CSG")
        ):
            result = other.halls(
                row,
                attributes(row["properties"]),
                info,
                info["facade_plan"],
                value["entry"],
                "industrial",
                owner.base.current_envelope,
                1,
                view_for(row),
            )
        self.assertIsNone(result)
        self.assertTrue(other.halls.rejected)

    def test_existing_small_five_and_fifteen_room_programmes_are_preserved(self):
        for (row, neighbors), expected in zip(roof_sources()[:2], (5, 15), strict=True):
            owner = FacadeDetails([row, *neighbors], view_for(row))
            _mesh, info = owner(row, attributes(row["properties"]), 1)
            self.assertEqual(info["rooms"], expected)
            self.assertFalse(owner.halls.planned)
            self.assertNotIn("hall_plan", info)

    def test_early_far_and_lookaway_defer_csg_keep_logical_state_and_plan(self):
        row, neighbors = sources()[2]
        _owner, _, info, *_ = construct(row, neighbors)
        view = view_for(row)
        distant = replace(view, eye=view.target + (view.eye - view.target) * 1000)
        away = replace(view, target=2 * view.eye - view.target)
        for current in (distant, away):
            other = FacadeDetails(
                [row, *neighbors],
                current,
                gate_states={row["id"] + ":floor-0:loading-0": 1},
            )
            with (
                patch(
                    "tile_industrial_halls.structure",
                    side_effect=AssertionError("unneeded CSG"),
                ),
                patch(
                    "tile_industrial_halls.products",
                    side_effect=AssertionError("unneeded parts"),
                ),
            ):
                _, deferred = other(row, attributes(row["properties"]), 1)
            self.assertEqual(deferred["hall_plan"], info["hall_plan"])
            self.assertEqual(deferred["hall_render"], "deferred")
            self.assertEqual(deferred["hall_state"]["opened"], 1)


if __name__ == "__main__":
    unittest.main()
