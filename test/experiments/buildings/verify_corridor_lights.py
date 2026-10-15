"""Physical fixture containment, identity and unchanged default lighting controls."""

import unittest

import numpy as np
import shapely
from interior_light_plan import lights


class CorridorLights(unittest.TestCase):
    def plan(self):
        floor = {
            "rooms": [shapely.box(0, 0, 12, 1.4), shapely.box(0, 2, 12, 5)],
            "room_kinds": ["corridor", "generic-room"],
            "ceiling_z_m": 3.2,
            "doors": [
                {
                    "id": f"door-{index}",
                    "rooms": [0, 1],
                    "centre": [x, y],
                    "tangent": [1, 0],
                }
                for index, (x, y) in enumerate([(1, 0), (1, 1.4), (5, 0), (10, 0)])
            ],
        }
        return {"storey_room_ids": [["hall", "office"]], "storey_plans": [floor]}

    def test_portal_fixtures_are_contained_stable_and_share_room_light_state(self):
        plan = self.plan()
        baseline = lights(plan)
        records = lights(plan, circulation_portals=True)
        added = [r for r in records if "portal_id" in r]
        self.assertEqual(len(added), 2)
        self.assertEqual(records, lights(plan, circulation_portals=True))
        self.assertEqual([r for r in records if r["room_id"] == "office"], baseline[1:])
        self.assertEqual(len({r["fixture_id"] for r in added}), len(added))
        for record in added:
            x, y, z = record["position"]
            w, h = np.asarray(record["size"]) / 2
            self.assertTrue(
                plan["storey_plans"][0]["rooms"][0].covers(
                    shapely.box(x - w, y - h, x + w, y + h)
                )
            )
            self.assertEqual(z, 3.16)
        self.assertTrue(
            all(not r["enabled"] for r in lights(plan, "off", circulation_portals=True))
        )


if __name__ == "__main__":
    unittest.main()
