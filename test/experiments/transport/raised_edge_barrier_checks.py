"""Physical side bodies, central passage, finite support and rejected local cases."""

import unittest
from pathlib import Path

import numpy as np
import shapely
from raised_edge_barriers import BarrierRecipe, construct


def bed_for(axis, width=3.8):
    return shapely.LineString(axis).buffer(
        width / 2, cap_style="flat", join_style="mitre", mitre_limit=2
    )


class SideBodies(unittest.TestCase):
    def test_actual_versioned_three_source_chains_without_private_outputs(self):
        from elevated_rail_hardware import chains
        from elevated_rail_inputs import read_sources
        from shared_bridge_geometry import corridor

        fixture = (
            Path(__file__).with_name("fixtures") / "stair-elevated-rail-vienna.json"
        )
        _recipe, _context, _originals, rows = read_sources(fixture, ())
        specs, omitted = chains(rows)
        self.assertEqual(len(rows), 3)
        self.assertEqual(len(specs), 2)
        self.assertEqual(omitted, [])
        bed = shapely.union_all([corridor(row) for row in rows])
        products = [
            item for spec in specs for item in construct(spec["xy"], 3.8, bed, 4)
        ]
        self.assertEqual(len(products), 4)
        self.assertEqual(sum(len(p["body"].vertices) for p in products), 80)
        self.assertEqual(sum(len(p["body"].triangles) for p in products), 144)

    def test_straight_closed_bodies_have_full_dimensions_and_open_track_ends(self):
        axis = [[0, 0], [20, 0]]
        rows = construct(axis, 3.8, bed_for(axis), 4)
        self.assertEqual(len(rows), 2)
        for row in rows:
            self.assertEqual(len(row["body"].triangles), 12)
            low, high = row["body"].vertices.min(0), row["body"].vertices.max(0)
            np.testing.assert_allclose(
                high - low, [19.9, 0.12, 0.9], rtol=0, atol=1e-14
            )
            for p in row["proof"]:
                self.assertAlmostEqual(p["volume_m3"], 19.9 * 0.12 * 0.9, places=5)
            self.assertEqual(
                row["domain"]
                .intersection(shapely.LineString([[0, -1.5], [0, 1.5]]))
                .length,
                0,
            )
            self.assertEqual(
                row["domain"]
                .intersection(shapely.LineString([[20, -1.5], [20, 1.5]]))
                .length,
                0,
            )

    def test_small_source_knick_preserves_continuous_whole_sides_and_central_strip(
        self,
    ):
        axis = [[0, 0], [10, 0], [20, 0.06], [40, 0.18]]
        rows = construct(axis, 3.8, bed_for(axis), 4)
        central = shapely.LineString(axis).buffer(
            1.5, cap_style="flat", join_style="mitre"
        )
        for row in rows:
            self.assertTrue(row["domain"].disjoint(central))
            upper = row["upper"]
            whole = shapely.coverage_union_all(
                shapely.polygons(upper.vertices[upper.triangles, :2])
            )
            self.assertTrue(whole.equals(row["domain"]))
            self.assertEqual(whole.geom_type, "Polygon")
            self.assertEqual(len(whole.interiors), 0)
        self.assertTrue(rows[0]["domain"].disjoint(rows[1]["domain"]))

    def test_actual_float32_xy_support_is_checked(self):
        axis = [[0, 4096.0001], [20, 4096.0001]]
        with self.assertRaisesRegex(ValueError, "Stored whole side body loses support"):
            construct(axis, 3.8, bed_for(axis), 4, BarrierRecipe(edge_margin_m=1e-7))

    def test_missing_support_hole_or_insufficient_height_is_rejected(self):
        axis = [[0, 0], [20, 0]]
        bed = bed_for(axis)
        for bad in (
            bed.difference(shapely.box(9, 1.7, 11, 1.9)),
            shapely.box(0, -1, 20, 1),
        ):
            with self.assertRaisesRegex(ValueError, "deck support"):
                construct(axis, 3.8, bad, 4)
        with self.assertRaisesRegex(ValueError, "reserved height"):
            construct(axis, 3.8, bed, 0.8)

    def test_stored_central_strip_cannot_shrink_with_float32_rounding(self):
        axis = [[0, 4096.0001], [20, 4096.0001]]
        with self.assertRaisesRegex(ValueError, "Stored side body consumes"):
            construct(
                axis, 3.8, bed_for(axis), 4, BarrierRecipe(clear_half_width_m=1.72999)
            )

    def test_no_unplanned_corner_or_loop_or_fold(self):
        for axis in (
            [[0, 0], [10, 0], [10, 10]],
            [[0, 0], [10, 0], [0, 0]],
            [[0, 0], [0, 0], [10, 0]],
            [[0, 0], [0.01, 0], [0.02, 0.0017]],
        ):
            with self.assertRaises(ValueError):
                construct(axis, 3.8, bed_for(axis), 4)

    def test_width_recipe_coordinates_and_station_budget_are_bounded(self):
        axis = [[0, 0], [20, 0]]
        with self.assertRaisesRegex(ValueError, "central clear strip"):
            construct(axis, 3, bed_for(axis), 4)
        for dimensions in (
            {"height_m": float("nan")},
            {"thickness_m": 0},
            {"edge_margin_m": -1},
        ):
            with self.assertRaises(ValueError):
                BarrierRecipe(**dimensions)
        for bad in (
            [[0, 0], [float("inf"), 0]],
            [[9000, 0], [9020, 0]],
            np.c_[np.arange(257), np.zeros(257)],
        ):
            with self.assertRaises(ValueError):
                construct(bad, 3.8, bed_for(axis), 4)


if __name__ == "__main__":
    unittest.main()
