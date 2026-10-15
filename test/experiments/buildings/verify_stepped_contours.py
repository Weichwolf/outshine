"""Explicit contour contracts retain source/core while rejecting unsupported ground/area."""

import unittest

import numpy as np
import shapely
from building_layout import WALL_M, canonical_geometry
from building_setbacks import apply
from building_terraces import apply as terraces
from building_vertical import build
from stepped_form_plan import outlines


class Contours(unittest.TestCase):
    def setUp(self):
        self.plan = build(
            dict(
                id="contour-control",
                polygon=shapely.box(0.0, 0.0, 18.0, 14.0),
                properties={"render_height": 9.0, "building:levels": 3},
            )
        )

    def test_ground_source_and_core_survive_actual_axis_cuts(self):
        requested = outlines(self.plan, np.array([1.0, 0.0]), 1, [0.0, 1.2, 2.4])
        result = apply(self.plan, [0.0, 1.2, 2.4], outlines=requested)
        self.assertTrue(
            result["storey_plans"][0]["footprint"].equals(self.plan["footprint"])
        )
        for index, current in enumerate(result["storey_plans"]):
            self.assertEqual(len(current["rooms"]), len(self.plan["rooms"]))
            self.assertGreaterEqual(min(room.area for room in current["rooms"]), 5.0)
            self.assertLessEqual(
                current["footprint"].difference(self.plan["footprint"]).area, 1e-7
            )
            if index:
                self.assertEqual(
                    current["footprint"]
                    .difference(result["storey_plans"][index - 1]["footprint"])
                    .area,
                    0.0,
                )
        self.assertEqual(result["storey_room_ids"], self.plan["storey_room_ids"])

    def test_invalid_explicit_contours_reject_without_changing_source(self):
        original = self.plan["footprint"].wkb
        invalid = [
            [],
            [shapely.box(1, 1, 17, 13)] * 3,
            [self.plan["footprint"], shapely.box(0, 0, 19, 14), self.plan["footprint"]],
            [self.plan["footprint"], shapely.Polygon(), self.plan["footprint"]],
        ]
        for shapes in invalid:
            with self.assertRaises(ValueError):
                apply(self.plan, [0.0, 1.2, 2.4], outlines=shapes)
        self.assertEqual(self.plan["footprint"].wkb, original)
        self.assertNotIn("storey_plans", self.plan)

    def test_zero_area_boundary_piece_keeps_room_area_without_inventing_access(self):
        wing = build(
            dict(
                id="wing-contour",
                polygon=shapely.Polygon(
                    [(0, 0), (22, 0), (22, 10), (12, 10), (12, 18), (0, 18)]
                ),
                properties={"render_height": 9.0, "building:levels": 3},
            )
        )
        stepped = apply(wing, [0.0, 0.8, 1.6])
        upper = stepped["storey_plans"][1]
        interior = upper["footprint"].buffer(-WALL_M, join_style="mitre")
        collections = []
        for room in wing["rooms"]:
            clipped = canonical_geometry(room.intersection(interior))
            if clipped.geom_type == "GeometryCollection":
                parts = [
                    p
                    for p in shapely.get_parts(clipped)
                    if p.geom_type in ("Polygon", "MultiPolygon")
                ]
                self.assertEqual(shapely.union_all(parts).area, clipped.area)
                collections.append(clipped)
        self.assertTrue(collections)
        declared = terraces(stepped)
        self.assertTrue(declared["terrace_access"])
        self.assertTrue(all(r["reason"] for r in declared["terrace_access"]))
        self.assertFalse(
            any(
                d["kind"] == "terrace"
                for current in declared["storey_plans"]
                for d in current["doors"]
            )
        )


if __name__ == "__main__":
    unittest.main()
