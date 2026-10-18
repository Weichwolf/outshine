"""Independent source occupation, camera-contact and water-hole controls."""

import unittest
from dataclasses import replace
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.places import PlaceCamera, tile_geography
from place_ground import terrain
from place_stand import sites
from place_water import water


def camera():
    return PlaceCamera(
        "control",
        0,
        0,
        0,
        0,
        10,
        False,
        0,
        0,
        60,
        (1280, 720),
        "2026-06-21T12:00:00Z",
        "",
        240000,
    )


class ConstantGround:
    def sample(self, *coordinates):
        if len(coordinates) == 2:
            return np.full(
                np.broadcast_shapes(*(np.shape(a) for a in coordinates)), 3.0
            )
        xy = coordinates[0]
        return SimpleNamespace(
            points_enu_m=np.column_stack((xy, np.full(len(xy), 3))),
            reasons=("",) * len(xy),
        )


def feature(geometry, kind):
    return {"id": 7, "properties": {"class": kind}, "geometry": geometry}


class PlaceSceneTests(unittest.TestCase):
    def test_curved_terrain_winding_and_declared_camera_clearance(self):
        model, sampler, report = terrain(camera(), ConstantGround(), 20000)
        p = model.vertices[model.triangles]
        normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        self.assertTrue((normals[:, 2] > 0).all())
        contact = sampler.sample([[0, 0]])
        self.assertEqual(contact.reasons, ("",))
        self.assertAlmostEqual(contact.points_enu_m[0, 2], 3, places=8)
        self.assertAlmostEqual(
            camera().eye()[2] - contact.points_enu_m[0, 2], 7, places=8
        )
        self.assertGreater(report["triangles"], 0)

    def test_source_sites_respect_building_and_road_and_ignore_camera_bearing(self):
        ring = [[0, 0], [4096, 0], [4096, 4096], [0, 4096], [0, 0]]
        building = [
            [1024, 1024],
            [2048, 1024],
            [2048, 2048],
            [1024, 2048],
            [1024, 1024],
        ]
        layer = {
            "extent": 4096,
            "features": [feature({"type": "Polygon", "coordinates": [ring]}, "wood")],
        }
        data = {
            "landcover": layer,
            "building": {
                "features": [
                    feature({"type": "Polygon", "coordinates": [building]}, "building")
                ]
            },
            "transportation": {
                "features": [
                    feature(
                        {
                            "type": "LineString",
                            "coordinates": [[0, 3072], [4096, 3072]],
                        },
                        "path",
                    )
                ]
            },
        }
        receipt = {"kind": "vector", "tile": [14, 8192, 8192], "sha256": "source"}
        with patch("place_stand.decode_vector", return_value=data):
            first, report = sites(camera(), [receipt], ConstantGround())
            second, _ = sites(
                replace(camera(), bearing_deg=180), [receipt], ConstantGround()
            )
        for key in first:
            np.testing.assert_array_equal(first[key], second[key])
        self.assertEqual(len(np.unique(first["ids"], axis=0)), len(first["positions"]))
        self.assertTrue((first["positions"][:, 2] == 3).all())
        self.assertTrue((first["source"] == 0).all())
        self.assertEqual(report["sources"][0]["source_sha256"], "source")
        lat, lon = tile_geography(building, 4096, receipt["tile"])
        exclusion = shapely.Polygon(camera().local(lat, lon, 0)[:, :2]).buffer(1.5)
        self.assertFalse(
            shapely.intersects(
                exclusion, shapely.points(first["positions"][:, :2])
            ).any()
        )
        lat, lon = tile_geography([[0, 3072], [4096, 3072]], 4096, receipt["tile"])
        road = shapely.LineString(camera().local(lat, lon, 0)[:, :2])
        self.assertTrue(
            (
                shapely.distance(road, shapely.points(first["positions"][:, :2])) > 2
            ).all()
        )

    def test_water_hole_is_not_triangulated_over(self):
        outer = [[0, 0], [100, 0], [100, 100], [0, 100], [0, 0]]
        hole = [[25, 25], [25, 75], [75, 75], [75, 25], [25, 25]]
        data = {
            "water": {
                "extent": 4096,
                "features": [
                    feature({"type": "Polygon", "coordinates": [outer, hole]}, "pond")
                ],
            }
        }
        receipt = {"kind": "vector", "tile": [14, 8192, 8192], "sha256": "water"}
        with patch("place_water.decode_vector", return_value=data):
            model, report = water(camera(), ConstantGround(), [receipt])
        lat, lon = tile_geography(hole, 4096, receipt["tile"])
        interior = shapely.Polygon(camera().local(lat, lon, 3.08)[:, :2])
        triangles = shapely.polygons(model.vertices[model.triangles, :2])
        self.assertLess(
            float(shapely.area(shapely.intersection(triangles, interior)).sum()), 1e-6
        )
        self.assertEqual(report["sources"][0]["height_m"], 3)


if __name__ == "__main__":
    unittest.main()
