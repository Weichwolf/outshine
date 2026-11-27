import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from bank_vegetation import bank_plants, reed, sedge
from shore_grade import shore_grade
from water_surface import WaterBody


class BankVegetationTest(unittest.TestCase):
    def test_wet_bank_plants_follow_the_slope_and_leave_the_path_and_water_clear(self):
        body = WaterBody(shapely.box(-8, -5, 0, 5), 0, water_class="pond")
        path = shapely.box(2, -1, 4, 1)
        shore = shore_grade((body,), shapely.box(-8, -5, 8, 5), path)
        source = SimpleNamespace(
            lon=16.4,
            lat=48.2,
            metres=np.array([74300, 111180]),
            datum=160,
            features=[
                ("landcover", shapely.box(0, -5, 8, 5), {"class": "wood"}),
                ("landcover", shapely.box(0, -4, 4, 0), {"class": "wetland"}),
                (
                    "landcover",
                    shapely.box(1.6, 3, 5, 5),
                    {"class": "grass", "subclass": "park"},
                ),
            ],
        )
        xy = np.array([[0, -5], [8, -5], [8, 5], [0, 5]])
        ground = np.column_stack((xy, 0.2 * xy[:, 0]))
        soil = {"grass": (ground, [(0, 1, 2), (0, 2, 3)])}
        candidates = np.array(
            [
                [-1, -2],
                [1.5, -2],
                [2.5, 0],
                [1, 2.5],
                [5.2, 2.5],
                [0.05, -2.5],
                [2, 3.5],
            ]
        )
        marks = np.tile([0.22, 0.35, 0.0], (len(candidates), 1))
        with patch("bank_vegetation.tile_sites", return_value=[(candidates, marks)]):
            plants = {
                name: vertices
                for name, vertices, _ in bank_plants(
                    soil, source, shore, np.array([2, 0, 1.7]), 18
                )
            }
        self.assertEqual(len(plants["reed culms"]), len(reed()["reed culms"][0]))
        self.assertEqual(len(plants["bank sedge"]), len(sedge()["bank sedge"][0]))
        self.assertAlmostEqual(plants["reed culms"][:, 2].min(), 0.286)
        for vertices in plants.values():
            self.assertTrue((vertices[:, 0] > 0).all())
            self.assertFalse(shapely.intersects_xy(path, *vertices[:, :2].T).any())
