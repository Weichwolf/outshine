"""Independent world-space fixture containment and actual Blender light states."""

import unittest

import bpy
import numpy as np
import shapely
from blender_room_night import configure
from building_layout import WALL_M
from building_plan import CEILING_M, FLOOR_M
from open_floor_lights import fixture_plan, light_state
from terraced_crown_plan import plan
from terraced_mass_sources import sources


class OpenFloorLights(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mass = plan(sources()[0], "cascade").body
        cls.fixtures = fixture_plan(cls.mass)

    def test_actual_world_cavities_contain_complete_fixture_rectangles(self):
        mass = self.mass
        for record in self.fixtures:
            storey = mass.storeys[record["storey"]]
            allowed = (
                shapely.from_wkb(storey.footprint_wkb)
                .buffer(-WALL_M, join_style="mitre")
                .difference(shapely.from_wkb(mass.core.footprint_wkb).buffer(0.20))
            )
            x, y, z = record["position"]
            width, height = record["size"]
            points = [
                np.array([x, y])
                + a * width / 2 * np.array(mass.core.across_xy)
                + b * height / 2 * np.array(mass.core.along_xy)
                for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1))
            ]
            self.assertTrue(allowed.covers(shapely.Polygon(points)))
            self.assertGreater(z, storey.bottom_m + FLOOR_M + 1.90)
            self.assertLess(z, storey.top_m - CEILING_M)
        self.assertEqual(
            {r["storey"] for r in self.fixtures}, set(range(len(mass.storeys)))
        )
        self.assertEqual(
            len({r["fixture_id"] for r in self.fixtures}), len(self.fixtures)
        )

    def test_states_keep_geometry_and_share_the_existing_floor_identity(self):
        self.assertEqual(self.fixtures, fixture_plan(self.mass))
        for mode in ("all-lit", "mixed", "off"):
            records = light_state(self.fixtures, mode)
            self.assertEqual(
                [r | {"enabled": False} for r in records],
                [r | {"enabled": False} for r in self.fixtures],
            )
            reverse = {
                r["fixture_id"]: r for r in light_state(self.fixtures[::-1], mode)
            }
            self.assertEqual(records, [reverse[r["fixture_id"]] for r in self.fixtures])
            for room in {r["room_id"] for r in records}:
                self.assertEqual(
                    len({r["enabled"] for r in records if r["room_id"] == room}), 1
                )
        self.assertTrue(
            all(r["enabled"] for r in light_state(self.fixtures, "all-lit"))
        )
        self.assertFalse(any(r["enabled"] for r in light_state(self.fixtures, "off")))
        mixed = light_state(self.fixtures, "mixed")
        self.assertTrue(any(r["enabled"] for r in mixed))
        self.assertTrue(any(not r["enabled"] for r in mixed))

    def test_bad_parameters_and_candidate_budget_reject(self):
        for kwargs in (
            {"spacing_m": 0},
            {"spacing_m": np.nan},
            {"power_w": -1},
            {"power_w": np.inf},
            {"max_candidates": 1},
        ):
            with self.assertRaises(ValueError):
                fixture_plan(self.mass, **kwargs)
        with self.assertRaises(ValueError):
            light_state(self.fixtures, "missing")

    def test_actual_blender_lights_match_stable_on_off_descriptors(self):
        for mode in ("all-lit", "off"):
            bpy.ops.wm.read_factory_settings(use_empty=True)
            scene = bpy.context.scene
            scene.world = bpy.data.worlds.new("night-control")
            sun = bpy.data.lights.new("sun", "SUN")
            obj = bpy.data.objects.new("sun", sun)
            scene.collection.objects.link(obj)
            report = {"room_light_plan": self.fixtures, "room_light_state": mode}
            settings = configure(
                scene,
                report,
                {
                    "world_linear_rgb": [0.35, 0.45, 0.6],
                    "view_transform": "AgX",
                    "exposure": 1.5,
                },
            )
            enabled = [r for r in settings["fixtures"] if r["enabled"]]
            actual = {
                o.name: o
                for o in scene.objects
                if o.type == "LIGHT" and o.data.type == "AREA"
            }
            self.assertEqual(set(actual), {r["fixture_id"] for r in enabled})
            self.assertEqual(sun.energy, 0)
            for record in enabled:
                lamp = actual[record["fixture_id"]]
                np.testing.assert_allclose(
                    lamp.location[:], record["position"], atol=1e-4
                )
                self.assertEqual(lamp.data.energy, record["power_w"])
                np.testing.assert_allclose(
                    lamp.data.color[:], record["colour_linear"], atol=1e-7
                )


if __name__ == "__main__":
    unittest.main()
