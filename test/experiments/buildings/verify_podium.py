"""Actual repeated upper masses, independent rebuilding and retained functional geometry."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from building_layout import WALL_M, rectangle
from building_vertical import level
from demand import View
from podium_plan import (
    DOOR_APPROACH_M,
    PodiumUnavailable,
    choose,
    constant_upper,
    programme,
    proposal,
)
from podium_products import prepare
from provider_contract import attributes
from shapely.affinity import rotate, translate
from stepped_profile_probe import sources
from stepped_source import base_plan, exterior
from tile_podiums import PodiumDetails, eligible
from tile_room_details import numeric_properties


def compare_xy(a, b):
    for x, y in zip(a["storey_plans"], b["storey_plans"], strict=True):
        for key in ("footprint", "walls", "walkable", "terrace", "terrace_guard"):
            if x[key].wkb != y[key].wkb:
                raise ValueError("Repeated source field differs: " + key)
        for room, other in zip(x["rooms"], y["rooms"], strict=True):
            if room.wkb != other.wkb:
                raise ValueError("Repeated source room differs")
        for window, other in zip(x["windows"], y["windows"], strict=True):
            if window["polygon"].wkb != other["polygon"].wkb or {
                k: v for k, v in window.items() if k != "polygon"
            } != {k: v for k, v in other.items() if k != "polygon"}:
                raise ValueError("Repeated opening differs")
        for key in ("base_z_m", "floor_z_m", "ceiling_z_m"):
            if x[key] != y[key]:
                raise ValueError("Repeated source elevation differs")


class Podiums(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = sources()
        cls.bases = [base_plan(row) for row in cls.rows]
        cls.products = [
            prepare(cls.rows[i], kind) for i, kind in ((2, "wing"), (3, "podium"))
        ]

    def test_analytic_rectangles_and_rotated_profiles(self):
        polygon = shapely.box(0, 0, 24, 18)
        p = {"footprint": polygon}
        self.assertAlmostEqual(proposal(p, "podium", 2).area, 20 * 14)
        for axis in (0, 1):
            actual = proposal(p, "wing", 2, axis)
            self.assertTrue(
                any(
                    actual.equals(box)
                    for box in (shapely.box(2, 0, 22, 18), shapely.box(0, 2, 24, 16))
                )
            )
        for kind in ("podium", "wing"):
            moved = translate(rotate(polygon, 31, origin=(0, 0)), 1300, -2100)
            expected = translate(
                rotate(proposal(p, kind, 2), 31, origin=(0, 0)), 1300, -2100
            )
            actual = proposal({"footprint": moved}, kind, 2)
            self.assertLess(expected.symmetric_difference(actual).area, 1e-9)

    def test_real_upper_profiles_are_exactly_constant_and_keep_source_holes_and_ids(
        self,
    ):
        for index, kind in ((0, "wing"), (1, "wing"), (2, "wing"), (3, "podium")):
            original = self.bases[index]
            before = original["footprint"].wkb
            p = choose(original, kind)
            b = p["form_recipe"]["base_storeys"]
            upper = p["storey_plans"][b]["footprint"]
            self.assertLess(upper.area, original["footprint"].area * 0.96)
            for i, current in enumerate(p["storey_plans"]):
                self.assertEqual(
                    current["footprint"].wkb, before if i < b else upper.wkb
                )
                self.assertEqual(current["terrace"].is_empty, i != b)
                self.assertLess(
                    current["footprint"].difference(original["footprint"]).area, 1e-9
                )
                self.assertEqual(len(current["rooms"]), len(original["rooms"]))
            self.assertEqual(p["storey_room_ids"], original["storey_room_ids"])
            self.assertEqual(p["vertical_portals"], original["vertical_portals"])
            self.assertEqual(original["footprint"].wkb, before)

    def test_one_template_equals_independent_rebuilding_on_every_real_storey(self):
        for index, kind in ((1, "wing"), (2, "wing"), (3, "podium")):
            a = choose(self.bases[index], kind)
            b = choose(self.bases[index], kind, reuse=False)
            compare_xy(a, b)
            first = a["form_recipe"]["base_storeys"]
            shared = a["storey_plans"][first]
            for current in a["storey_plans"][first + 1 :]:
                for key in ("footprint", "rooms", "walls"):
                    self.assertIs(current[key], shared[key])

    def test_real_door_approaches_are_reserved_before_extrusion(self):
        p = self.products[1].plan
        base = p["form_recipe"]["base_storeys"]
        source = level(self.bases[3], base)
        upper = p["storey_plans"][base]["footprint"]
        for door in source["doors"]:
            pad = rectangle(
                door["centre"],
                np.asarray(door["tangent"]),
                0.6,
                DOOR_APPROACH_M + 0.3,
                DOOR_APPROACH_M + 0.3,
            )
            self.assertLess(
                pad.intersection(self.bases[3]["footprint"]).difference(upper).area,
                1e-7,
            )

        def old_core(plan, _index):
            return shapely.union_all(
                [plan["core_envelope"], plan["corridor_zone"]]
            ).buffer(WALL_M, join_style="mitre")

        with (
            patch("podium_plan.reserved", old_core),
            self.assertRaisesRegex(PodiumUnavailable, "torso"),
        ):
            prepare(self.rows[3], "podium")

    def test_prepared_products_keep_closed_height_correct_hulls_and_real_access(self):
        for asset, index in zip(self.products, (2, 3), strict=True):
            p = asset.plan
            mesh, proof, paths, _light = asset.physical
            source = self.rows[index]
            self.assertGreater(proof["sampled_poses"], 100)
            self.assertGreaterEqual(proof["sampled_min_headroom_m"], 1.9)
            self.assertTrue(all(r["headroom_m"] >= 1.9 for r in paths))
            self.assertTrue(paths)
            compact, _env, _closed, _method = exterior(p, 1)
            for product in (mesh, compact):
                self.assertEqual(
                    product["vertices"][:, 2].min(),
                    source["properties"]["render_min_height"],
                )
                self.assertEqual(
                    product["vertices"][:, 2].max(),
                    source["properties"]["render_height"],
                )
            with self.assertRaises(TypeError):
                p["source_top_m"] = -1
            self.assertFalse(mesh["vertices"].flags.writeable)
            self.assertIs(mesh["vertices"], asset.near_draw["vertices"])

    def test_bad_requests_reject_before_heavy_expansion(self):
        with (
            patch("podium_plan.base_plan", side_effect=AssertionError("expanded")),
            self.assertRaisesRegex(ValueError, "podium or wing"),
        ):
            programme({}, "unknown")
        for invalid in (True, 0, -1, float("nan")):
            with self.assertRaises(ValueError):
                proposal({"footprint": shapely.box(0, 0, 20, 20)}, "podium", invalid)
        for proposed in (shapely.LineString([(0, 0), (1, 1)]), shapely.Polygon()):
            with self.assertRaises(ValueError):
                constant_upper(self.bases[2], proposed, 2)
        for count in (True, 0, self.bases[2]["storeys"] - 1):
            with self.assertRaises(ValueError):
                constant_upper(self.bases[2], self.bases[2]["footprint"], count)

    def test_nonadmitted_real_source_keeps_its_geometry_gate_and_failure_reason(self):
        with self.assertRaises(PodiumUnavailable) as caught:
            prepare(self.rows[0], "wing")
        failures = [
            r for r in caught.exception.attempts if r["status"] == "rejected-product"
        ]
        self.assertTrue(failures)
        self.assertTrue(any("Mismatched" in r["reason"] for r in failures))

    def test_source_selection_and_warm_cameras_use_only_prepared_products(self):
        rows = [
            dict(
                self.rows[i],
                layer="building",
                clipped=False,
                context={"site_use": {"value": "residential"}},
            )
            for i in (2, 3)
        ]
        assets = {r["id"]: a for r, a in zip(rows, self.products, strict=True)}
        centre = rows[0]["polygon"].centroid
        view = View(
            np.array([centre.x + 3000, centre.y - 3000, 3000]),
            np.array([centre.x, centre.y, 10]),
            width=1920,
            height=1080,
        )
        with (
            patch("tile_podiums.seed_for", return_value=0),
            patch(
                "tile_podiums.prepare", side_effect=lambda row, kind: assets[row["id"]]
            ),
        ):
            owner = PodiumDetails(rows, view)
        self.assertEqual(set(owner.assets), set(assets))
        asset = assets[rows[0]["id"]]
        row = owner.source_rows[rows[0]["id"]]
        parameters = attributes(numeric_properties(row))
        inside = asset.plan["storey_plans"][0]["rooms"][0].representative_point()
        near = View(
            np.array([inside.x, inside.y, 1.7]),
            np.array([inside.x + 1, inside.y, 1.7]),
            width=1920,
            height=1080,
        )
        with patch("tile_podiums.prepare", side_effect=AssertionError("warm rebuild")):
            for actual, expected in (
                (view, asset.compact_draw),
                (near, asset.near_draw),
                (view, asset.compact_draw),
            ):
                owner.view = actual
                mesh, info = owner(row, parameters, 17)
                self.assertIs(mesh["vertices"], expected["vertices"])
                self.assertEqual(info["room_ids"], asset.plan["storey_room_ids"])
        with self.assertRaisesRegex(ValueError, "immutable prepared source"):
            owner(rows[0], parameters, 17)
        self.assertFalse(
            eligible(
                dict(
                    rows[0],
                    properties=dict(rows[0]["properties"], **{"roof:shape": "hip"}),
                )
            )
        )


if __name__ == "__main__":
    unittest.main()
