"""Whole-body projection, height translation and local post-generation rejection."""

import copy
import json
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.contracts import HeightInterval, Mesh, Reservation
from stair_consumer_buildings import body_query, one
from stair_consumer_parts import verify_parts
from stair_occupancy_geometry import OccupancyError
from stair_occupancy_handoff import OccupancyQuery


def body():
    vertices = np.array([[0, 0, -0.2], [2, 0, 0], [0, 2, 0], [0, 0, 3.4]])
    return Mesh(vertices, np.array([[0, 2, 1], [0, 1, 3], [1, 2, 3], [0, 3, 2]]))


class ConsumerChecks(unittest.TestCase):
    def test_real_clipped_dust_has_no_constructible_prism_faces(self):
        from building_mesh import solid
        from tile_buildings import prism

        path = (
            Path(__file__).parents[2]
            / "outshine/data/transport/building-cut-dust-case.json"
        )
        case = json.loads(path.read_text())
        polygon = shapely.from_wkb(case["polygon_wkb"])
        self.assertTrue(polygon.is_valid)
        self.assertGreater(polygon.area, 0)
        self.assertLess(polygon.area, 1e-14)
        self.assertEqual(polygon.area, case["area_m2"])
        storage = ([], [], [], [])
        solid(
            storage,
            polygon,
            case["parameters"]["minHeight"],
            case["parameters"]["buildingHeight"],
            case["material"],
        )
        self.assertEqual(len(storage[1]), 0)
        with self.assertRaisesRegex(ValueError, "prism triangulation"):
            prism({"polygon": polygon}, case["parameters"], case["material"])

    def test_empty_and_collinear_prisms_defer_while_finite_prism_stays_valid(self):
        from tile_buildings import prism

        parameters = {"minHeight": 0, "buildingHeight": 4}
        for polygon in (shapely.Polygon(), shapely.Polygon([(0, 0), (1, 0), (2, 0)])):
            with (
                self.subTest(polygon=polygon.wkt),
                self.assertRaisesRegex(ValueError, "prism triangulation"),
            ):
                prism({"polygon": polygon}, parameters, 1)
        mesh, _info = prism({"polygon": shapely.box(0, 0, 2, 3)}, parameters, 1)
        self.assertGreater(len(mesh["faces"]), 0)
        self.assertTrue(np.isfinite(mesh["vertices"]).all())
        self.assertTrue(np.isfinite(mesh["normals"]).all())

    def test_unbuildable_cut_part_retains_other_actual_body_and_accounts_for_both(self):
        row = {"id": "original", "polygon": shapely.box(0, 0, 6, 2)}
        road = shapely.box(2.5, -1, 3.5, 3)
        query = OccupancyQuery(
            (Reservation("road", road, HeightInterval(-1, 5), "body"),)
        )
        contact = {"world_frame": "world", "root_enu_m": [0, 0, 0]}

        def generated(actual, _neighbors, _view):
            if actual["polygon"].bounds[0] > 3:
                raise ValueError("unconstructible prism")
            return body(), {}

        with (
            patch("stair_consumer_buildings.root_contact", return_value=contact),
            patch("stair_consumer_buildings.generated", side_effect=generated),
        ):
            _before, bodies, reservations, result = one(row, [], {}, query, None)
        self.assertEqual(len(bodies), 1)
        self.assertEqual(len(reservations), 1)
        self.assertEqual(len(result["products"]), 1)
        self.assertEqual(len(result["unbuilt_parts"]), 1)
        remaining = row["polygon"].difference(road)
        verify_parts(remaining, result, row["id"])
        for mutation in (
            "omitted",
            "duplicate",
            "wrong_source",
            "no_reason",
            "wrong_geometry",
        ):
            changed = copy.deepcopy(result)
            if mutation == "omitted":
                changed.pop("unbuilt_parts")
            elif mutation == "duplicate":
                changed["unbuilt_parts"].append(changed["products"][0])
            elif mutation == "wrong_source":
                changed["unbuilt_parts"][0]["source_part_id"] = "other"
            elif mutation == "no_reason":
                changed["unbuilt_parts"][0]["reason"] = ""
            else:
                changed["unbuilt_parts"][0]["footprint_wkb"] = row["polygon"].wkb_hex
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                verify_parts(remaining, changed, row["id"])

    def test_band_and_projection_cover_actual_recipe_and_keep_local_arrays(self):
        local = body()
        original = local.vertices.copy()
        world, footprint = body_query("source-part", local, 162.5)
        np.testing.assert_array_equal(local.vertices, original)
        np.testing.assert_array_equal(world.vertices, original + [0, 0, 162.5])
        self.assertEqual(footprint.heights, HeightInterval(162.3, 165.9))
        self.assertTrue(
            footprint.geometry.equals(shapely.Polygon([[0, 0], [2, 0], [0, 2]]))
        )

    def test_pure_height_touch_is_free_but_actual_roof_height_conflicts(self):
        _, footprint = body_query("source-part", body(), 162.5)
        for lo, hi, wanted in ((165.9, 167, False), (165.8, 167, True)):
            query = OccupancyQuery(
                (
                    Reservation(
                        "bridge", footprint.geometry, HeightInterval(lo, hi), "body"
                    ),
                )
            )
            self.assertEqual(not query.conflict(footprint)[0].is_empty, wanted)

    def test_generated_overhang_cannot_pass_only_a_source_footprint_check(self):
        row = {"id": "original", "polygon": shapely.box(0, 0, 0.1, 0.1)}
        query = OccupancyQuery(
            (
                Reservation(
                    "road", shapely.box(1, 0, 2, 1), HeightInterval(162, 167), "body"
                ),
            )
        )
        contact = {"world_frame": "world", "root_enu_m": [0, 0, 162.5]}
        with (
            patch("stair_consumer_buildings.root_contact", return_value=contact),
            patch("stair_consumer_buildings.generated", return_value=(body(), {})),
            self.assertRaisesRegex(OccupancyError, "still enters transport"),
        ):
            one(row, [], {}, query, None)


if __name__ == "__main__":
    unittest.main()
