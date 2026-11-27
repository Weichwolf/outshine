import json
import unittest
from importlib.resources import files
from types import SimpleNamespace

import numpy as np
import shapely
from outshine_ref.vegetation import PlantPlan
from scene_mesh import terrain_grid, terrain_height
from woodland_stem import rooted_stem


class WoodlandTest(unittest.TestCase):
    def test_flared_stem_follows_a_slope_without_entering_the_path(self):
        recipe = files("outshine_ref.vegetation").joinpath("data", "stand-oak.json")
        plan = PlantPlan.from_recipe(json.loads(recipe.read_text()))
        source = SimpleNamespace(height=lambda xy: 0.18 * xy[:, 0] - 0.21 * xy[:, 1])
        terrain, _ = terrain_grid(source, 8)
        domain = shapely.box(-4, -4, 0.9, 4)
        position = np.array([0.0, 0.0, -0.08])
        stem = rooted_stem(plan, domain, terrain, position, 0.62, 1.6, oak=True)

        vertices, faces = stem["vertices"], stem["faces"]
        collar = vertices[faces[faces[:, 0] == len(vertices) - 2, 1]]
        ground = terrain_height(terrain, collar[:, :2])
        self.assertTrue(domain.covers(shapely.Polygon(collar[:, :2])))
        np.testing.assert_allclose(collar[:, 2] - ground, -0.08, atol=1e-12)
        self.assertGreater(np.linalg.norm(collar[:, :2], axis=1).max(), 0.5)

        edges = np.sort(faces[:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2), axis=1)
        _, counts = np.unique(edges, axis=0, return_counts=True)
        np.testing.assert_array_equal(counts, 2)
