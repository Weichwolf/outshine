"""Physical body bands, crowns beyond free roots and explicit flat/world placement."""

import unittest
from types import SimpleNamespace
from unittest.mock import Mock

import numpy as np
import shapely
from outshine_ref.contracts import HeightInterval, LocalFrame, Reservation
from recipe_graph_data import plain
from recipe_plant_operators import SITE, prepared
from recipe_plant_sources import DATUM
from recipe_plant_values import seed_input
from stair_occupancy_geometry import OccupancyError
from stair_occupancy_handoff import OccupancyQuery
from stair_occupancy_inputs import volume
from stair_plant_geometry import bounds_query
from stair_plant_sites import candidate, plants
from stair_shared_headroom_volumes import upper_band


class FlatTerrain:
    def sample(self, xy):
        return SimpleNamespace(
            reasons=("",),
            points_enu_m=np.c_[xy, [10.0]],
            face_indices=np.array([3]),
            normals_enu=np.array([[0, 0, 1.0]]),
        )


class PlantChecks(unittest.TestCase):
    def setUp(self):
        self.support = shapely.box(-5, -5, 5, 5)
        self.row = {
            "id": "source:part:0",
            "source_id": "original",
            "part": 0,
            "polygon": self.support,
            "properties": {"class": "wood"},
        }
        self.data = {"frame": LocalFrame("world"), "sampler": FlatTerrain()}
        self.bounds = [[-1.0, -1.0, -0.1], [1.0, 1.0, 3.0]]

    def test_band_contains_every_actual_translated_bound(self):
        query = bounds_query("plant", self.bounds, [23, 19, 162])
        self.assertEqual(query.heights, HeightInterval(161.9, 165))
        self.assertTrue(query.geometry.equals(shapely.box(22, 18, 24, 20)))
        with self.assertRaisesRegex(ValueError, "finite"):
            bounds_query("plant", self.bounds, [0, 0, float("nan")])

    def test_free_root_cannot_admit_a_crown_crossing_transport(self):
        road = Reservation(
            "road", shapely.box(0.5, -0.2, 1.5, 0.2), HeightInterval(10, 14), "traffic"
        )
        self.assertFalse(road.geometry.covers(shapely.Point(0, 0)))
        graph = Mock()
        with self.assertRaisesRegex(OccupancyError, "whole measured plant envelope"):
            candidate(
                self.row,
                np.array([0.0, 0.0]),
                (0, 0, 0, 0, 0),
                self.support,
                self.data,
                OccupancyQuery((road,)),
                {"bounds_m": self.bounds},
                ((-5, -5), 16, "region"),
                graph,
            )
        graph.evaluate.assert_not_called()

    def test_crown_above_old_walking_band_still_hits_shared_car_headroom(self):
        top = np.array([[0, -1, 10], [2, -1, 10], [2, 1, 10]], float)
        physical, _ = volume("footway", top - [0, 0, 0.3], top, 2.5)
        extra = upper_band(physical, physical.geometry, 2, "logical-car")
        bounds = [[-1, -1, 0], [1, 1, 3]]
        origin = np.array([2.5, 0, 13.0])
        body = bounds_query("overhanging-crown", bounds, origin)
        self.assertFalse(physical.geometry.covers(shapely.Point(origin[:2])))
        self.assertTrue(OccupancyQuery((physical,)).conflict(body)[0].is_empty)
        combined = OccupancyQuery((physical, extra))
        self.assertFalse(combined.conflict(body)[0].is_empty)
        touch = bounds_query("ceiling-touch", bounds, [2.5, 0, 14.5])
        self.assertTrue(combined.conflict(touch)[0].is_empty)
        raised = SimpleNamespace(
            sample=lambda xy: SimpleNamespace(
                reasons=("",),
                points_enu_m=np.c_[xy, [13.0]],
                face_indices=np.array([3]),
                normals_enu=np.array([[0, 0, 1.0]]),
            )
        )
        graph = Mock()
        with self.assertRaisesRegex(OccupancyError, "whole measured plant envelope"):
            candidate(
                self.row,
                origin[:2],
                (0, 0, 0, 0, 0),
                self.support,
                dict(self.data, sampler=raised),
                combined,
                {"bounds_m": bounds},
                ((-5, -5), 16, "region"),
                graph,
            )
        graph.evaluate.assert_not_called()

    def test_actual_site_keeps_flat_metadata_and_explicit_world_transform(self):
        recipe = plain(
            prepared()
            .evaluate({"seed": seed_input(718)}, outputs=("plant",))
            .outputs["plant"]
            .value
        )
        far = Reservation(
            "far", shapely.box(50, 50, 60, 60), HeightInterval(0, 100), "body"
        )
        value = candidate(
            self.row,
            np.array([0.0, 0.0]),
            (0, 0, 0, 0, 0),
            self.support,
            self.data,
            OccupancyQuery((far,)),
            {"bounds_m": self.bounds, "recipe": recipe},
            ((-5, -5), 16, "region"),
            prepared(SITE),
        )
        self.assertEqual(value["placement"]["origin_m"], [0, 0, 0])
        self.assertEqual(value["world_origin_m"], [0, 0, 10])
        self.assertEqual(value["placement_metadata"]["height_datum"], DATUM)
        self.assertEqual(value["placement"]["source_id"], "original")
        with self.assertRaisesRegex(ValueError, "bounded"):
            plants({}, None, {}, max_attempts=-1)


if __name__ == "__main__":
    unittest.main()
