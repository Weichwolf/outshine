"""Actual courtyard-wall failures with unchanged finite actor collision limits."""

import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from building_plan import FLOOR_M
from terraced_mass_admission import inspect
from terraced_mass_plan import plan as mass_plan
from terraced_terrace_access import continuous, selected_parts
from terraced_terrace_geometry import TerraceProducts
from terraced_terrace_plan import plan


class GroundApproachTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).with_name("data") / "terraced-ground-controls.json"
        cls.rows = json.loads(data.read_text())["cases"]
        cls.owners = [
            TerraceProducts(
                plan(
                    mass_plan(
                        dict(r, polygon=shapely.from_wkb(r["footprint_wkb"])),
                        r["programme"],
                    )
                )
            )
            for r in cls.rows
        ]

    def test_previous_routes_still_hit_actual_unchanged_courtyard_walls(self):
        for row, owner in zip(self.rows, self.owners, strict=True):
            with self.subTest(source=row["id"]):
                path = shapely.from_wkb(row["original_approach_wkb"])
                floor = owner.plan.prepared.mass.bottom_m + FLOOR_M
                with self.assertRaisesRegex(ValueError, "continuous actor tube"):
                    continuous(selected_parts(owner, path, floor), path, floor)

    def test_new_routes_pass_all_existing_independent_physical_checks(self):
        self.assertEqual(len(self.owners), 2)
        for row, owner in zip(self.rows, self.owners, strict=True):
            with self.subTest(source=row["id"]):
                mass = owner.plan.prepared.mass
                old = shapely.from_wkb(row["original_approach_wkb"])
                new = shapely.from_wkb(mass.core.approach_wkb)
                np.testing.assert_array_equal(
                    np.asarray(old.coords)[[0, -1]], np.asarray(new.coords)[[0, -1]]
                )
                self.assertEqual(mass.source_wkb, bytes.fromhex(row["footprint_wkb"]))
                self.assertNotEqual(new.wkb, old.wkb)
                result = inspect(owner)
                self.assertGreater(result["ground_approach"]["critical_slabs"], 0)
                self.assertGreater(len(result["terraces"]), 0)


if __name__ == "__main__":
    unittest.main()
