"""Actual-source LOD, retained previous parts and paired material/source boundaries."""

import copy
import unittest
from unittest.mock import patch

import numpy as np
from demand import View
from terraced_admission_probe import controls
from terraced_city_parts import PreviousParts
from terraced_city_reference import admitted
from terraced_mass_census import source_identity
from tile_terraced import TerracedDetails, bound, definitions


class Previous:
    def __init__(self, rows):
        self.source_rows = {r["id"]: r for r in rows}
        r = rows[0]
        xy = np.asarray(r["polygon"].centroid.coords[0])
        self.centre = np.r_[xy, 12]
        self.view = View(
            self.centre + [0, -3000, 1000], self.centre, width=1920, height=1080
        )
        self.calls = 0

    def __call__(self, row, parameters, material):
        self.calls += 1
        return {
            "vertices": np.array([[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]]),
            "faces": np.array([[0, 1, 2]], dtype=np.uint32),
            "normals": np.tile([0.0, 0.0, 1.0], (3, 1)),
            "uv": np.zeros((3, 2)),
            "materials": np.array([material], dtype=np.uint16),
        }, {"form": "unchanged previous control"}


class CityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = controls()[:2]

    def test_unselected_source_reuses_exact_previous_part_without_new_modelling(self):
        previous = Previous(self.rows)
        parts = PreviousParts(previous)
        row = self.rows[1]
        before = parts(row, {}, 14)
        builder = TerracedDetails(parts, {})
        with patch(
            "tile_terraced.mass_plan",
            side_effect=AssertionError("unrequested new mass"),
        ):
            after = builder(row, {}, 14)
        self.assertIs(after, before)
        self.assertEqual(previous.calls, 1)
        self.assertFalse(builder.assets)
        with self.assertRaisesRegex(ValueError, "wall binding"):
            parts(row, {}, 15)

    def test_actual_far_source_avoids_near_then_movement_reuses_one_plan_and_near_asset(
        self,
    ):
        previous = Previous(self.rows)
        row = self.rows[0]
        record = source_identity(row) | {
            "programme": row["programme"],
            "checks": {"scope": "test selection only"},
        }
        builder = TerracedDetails(PreviousParts(previous), {row["id"]: record})
        with patch(
            "terraced_facade_geometry.build",
            side_effect=AssertionError("unrequested physical expansion"),
        ):
            _mesh, info = builder(row, {}, 14)
        self.assertEqual(info["product"], "compact")
        asset = builder.assets[row["id"]]
        self.assertIsNone(asset.facade.near)
        previous.view = View(
            previous.centre + [0, -20, 10], previous.centre, width=1920, height=1080
        )
        _mesh, info = builder(row, {}, 14)
        self.assertEqual(info["product"], "near")
        physical = asset.facade.near
        with patch(
            "terraced_facade_products.build_facade",
            side_effect=AssertionError("repeated physical expansion"),
        ):
            builder(row, {}, 14)
        self.assertIs(builder.assets[row["id"]], asset)
        self.assertIs(asset.facade.near, physical)
        with self.assertRaisesRegex(ValueError, "prepared source"):
            builder(dict(row), {}, 14)

    def test_component_binding_keeps_source_wall_and_separate_frame_roles_without_copying_geometry(
        self,
    ):
        original = {
            "materials": np.array([12, 9, 10, 11, 3, 4, 5], dtype=np.uint16),
            "vertices": np.zeros((3, 3)),
        }
        result = bound(original, 14)
        np.testing.assert_array_equal(
            result["materials"], [14, 60000, 60001, 60002, 3, 4, 5]
        )
        np.testing.assert_array_equal(original["materials"], [12, 9, 10, 11, 3, 4, 5])
        self.assertIs(result["vertices"], original["vertices"])
        self.assertEqual(set(definitions()), {"60000", "60001", "60002"})

    def test_reference_rejects_changed_source_failed_and_protected_memberships(self):
        rows = self.rows
        original = [source_identity(r) | {"status": "direct-candidate"} for r in rows]
        outcomes = [
            source_identity(r) | {"passed": i == 0, "programme": r["programme"]}
            for i, r in enumerate(rows)
        ]
        reference = {
            "completed": True,
            "original_census": original,
            "records": outcomes,
            "passed": 1,
        }
        self.assertEqual(
            set(admitted(rows, reference, {"existing_function_ids": []})),
            {rows[0]["id"]},
        )
        changed = copy.deepcopy(reference)
        changed["original_census"][0]["source_properties_sha256"] = "changed"
        with self.assertRaisesRegex(ValueError, "geometry/properties"):
            admitted(rows, changed, {"existing_function_ids": []})
        with self.assertRaisesRegex(ValueError, "functional ownership"):
            admitted(rows, reference, {"existing_function_ids": [rows[0]["id"]]})
        with self.assertRaisesRegex(ValueError, "membership"):
            admitted(rows[:-1], reference, {"existing_function_ids": []})
        changed = copy.deepcopy(reference)
        changed["records"][1]["passed"] = "false"
        with self.assertRaisesRegex(ValueError, "outcome"):
            admitted(rows, changed, {"existing_function_ids": []})


if __name__ == "__main__":
    unittest.main()
