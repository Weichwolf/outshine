"""Whole real curved street caps, saved original bindings and rigid lane bodies."""

import copy
import tempfile
import unittest
from pathlib import Path

import numpy as np
import shapely
from shared_bridge_geometry import corridor
from stair_cycle_contacts import profile
from stair_module_audit import audit
from stair_module_experiment import write_json
from stair_module_geometry import build, old_surfaces
from stair_module_inputs import FIXTURE, inputs
from stair_module_verify import saved_planning
from stair_road_vehicle import curve_sweep
from stair_southwest_checks import cached_reference


class RealCurvedStreets(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipe, cls.rows, _context, cls.sources, sample, _layer = inputs(
            FIXTURE.with_name("stair-southwest-curves-vienna.json")
        )
        cls.state = cached_reference()
        cls.old, cls.line, cls.ground, cls.upper = old_surfaces(
            cls.state, cls.recipe, cls.sources
        )
        cls.data, cls.note, cls.planning = build(
            cls.rows,
            cls.sources,
            cls.old,
            cls.line,
            cls.ground,
            sample,
            contact_mode="profile",
            junction_mode="level_box",
        )
        cls.proof = audit(
            cls.state, cls.recipe, cls.data, cls.rows, cls.planning, cls.line, cls.upper
        )

    def test_whole_original_axes_tags_and_endpoint_frames_survive(self):
        _r, original, *_rest = inputs(
            FIXTURE.with_name("stair-southwest-streets-vienna.json")
        )
        for before, after in zip(original, self.rows, strict=True):
            np.testing.assert_array_equal(before["xy"], after["xy"])
            self.assertEqual(before["original"], after["original"])
            for end in (0, -1):
                np.testing.assert_array_equal(profile(before, end), profile(after, end))
            self.assertLess(corridor(before).difference(corridor(after)).area, 1e-8)
        plans = [r["horizontal_curve_plan"] for r in self.rows[:4]]
        self.assertEqual(sum(len(p["corners"]) for p in plans), 5)
        self.assertTrue(
            all(c["chord_bound_m"] <= 0.025 for p in plans for c in p["corners"])
        )

    def test_whole_actual_curved_caps_and_both_lane_bodies_pass_double_and_float32(
        self,
    ):
        proof = self.proof["horizontal_curves"]
        checks = proof["checks"]
        self.assertEqual(len(checks), 20)
        self.assertTrue(all(c["fits_surface"] for c in checks))
        self.assertEqual({c["float32"] for c in checks}, {False, True})
        self.assertTrue(
            all(c["accepted_intervals"] == c["hull_intervals"] for c in checks)
        )
        self.assertGreaterEqual(
            min(c["minimum_rear_axle_radius_m"] for c in checks), 28.624999
        )
        self.assertEqual(self.proof["profiles"]["complete_additional_profiles"], 10)
        self.assertEqual(self.proof["profiles"]["complete_cached_interior_profiles"], 1)

    def test_wide_vehicle_cannot_pass_merely_from_centerline_containment(self):
        row = self.rows[3]
        corner = row["horizontal_curve_plan"]["corners"][1]
        actual = shapely.union_all(
            shapely.polygons(
                self.data["surface_vertices"][
                    self.data["surface_faces"][self.data["owners"][:, 3]], :2
                ]
            )
        )
        lane = shapely.LineString(row["surface_axis_xy_m"]).buffer(
            row["width_m"] / 2, single_sided=True, join_style="mitre"
        )
        vehicle = dict(self.proof["horizontal_curves"]["vehicle"], widthM=3.0)
        with self.assertRaisesRegex(ValueError, "whole compact vehicle leaves"):
            curve_sweep(actual.intersection(lane), row, corner, 1, True, vehicle)

    def test_saved_reader_rejects_a_shifted_derived_axis_with_originals_unchanged(self):
        rows = [
            {
                k: v.tolist() if isinstance(v, np.ndarray) else v
                for k, v in row.items()
                if k != "original"
            }
            for row in self.rows
        ]
        saved = {
            "rows": rows,
            "nodes": self.planning[0],
            "masks_wkt": [
                shapely.to_wkt(p, rounding_precision=-1) for p in self.planning[3]
            ],
        }
        with tempfile.TemporaryDirectory(
            prefix="outshine-transport-curves-test-"
        ) as path:
            output = Path(path)
            write_json(output / "planning.json", saved)
            saved_planning(output, self.rows, self.ground)
            changed = copy.deepcopy(saved)
            changed["rows"][3]["surface_axis_xy_m"][10][0] += 0.001
            write_json(output / "planning.json", changed)
            with self.assertRaisesRegex(
                ValueError, "derived horizontal road axis changed"
            ):
                saved_planning(output, self.rows, self.ground)


if __name__ == "__main__":
    unittest.main()
