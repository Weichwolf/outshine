import json
import unittest
from functools import partial
from importlib.resources import files
from types import SimpleNamespace

import numpy as np
import shapely
from outshine_ref.vegetation import PlantPlan
from outshine_ref.vegetation.curve_frame import arc_frame
from scene_mesh import terrain_grid, terrain_height
from woodland import represented_leaves, stand_plan, woodland_surface
from woodland_stem import lean_tree, rooted_stem, template_stem


class WoodlandTest(unittest.TestCase):
    def test_distant_canopy_preserves_leaf_area_with_fewer_representatives(self):
        plan = stand_plan("oak", 0)
        rows = plan.tables(sum(plan.axis_counts), sum(plan.leaf_counts))
        areas = np.array([blade.area() for blade in plan.blades])
        variants = rows["leaf_variants"]
        middle, _, _ = represented_leaves(rows, areas, 16)
        far, scale, centres = represented_leaves(rows, areas, 32)
        self.assertLess(len(far), len(middle))
        self.assertEqual(len(centres), len(far))
        self.assertAlmostEqual(
            np.sum(areas[variants[far]] * scale**2), sum(areas[variants])
        )

    def test_roots_follow_the_rendered_soil_across_a_grass_boundary(self):
        vertices = [[0, 0, 0], [2, 0, 1], [2, 2, 2], [0, 2, 1], [0, 0, -2]]
        surface = woodland_surface(
            {
                "ground": (vertices, [[0, 1, 2], [0, 4, 1]]),
                "grass": (vertices, [[0, 2, 3]]),
                "water": ([[0, 0, 3], [2, 0, 3], [2, 2, 3]], [[0, 1, 2]]),
            }
        )
        z = surface.sample([[1.5, 0.5], [1, 1], [0.5, 1.5]]).points_enu_m[:, 2]
        np.testing.assert_allclose(z, 1.0)

    def test_revised_stands_keep_the_authored_leaf_area(self):
        for species in ("oak", "beech"):
            data = files("outshine_ref.vegetation").joinpath(
                "data", f"stand-{species}.json"
            )
            base = PlantPlan.from_recipe(json.loads(data.read_text()))
            for form in (0, 1):
                with self.subTest(species=species, form=form):
                    area = stand_plan(species, form).summary()["leaf_area_m2"]
                    self.assertGreaterEqual(area, 0.95 * base.summary()["leaf_area_m2"])

    def test_flared_stem_follows_a_slope_without_entering_the_path(self):
        recipe = files("outshine_ref.vegetation").joinpath("data", "stand-oak.json")
        plan = PlantPlan.from_recipe(json.loads(recipe.read_text()))
        source = SimpleNamespace(height=lambda xy: 0.18 * xy[:, 0] - 0.21 * xy[:, 1])
        terrain, _ = terrain_grid(source, 8)
        domain = shapely.box(-4, -4, 0.9, 4)
        position = np.array([0.0, 0.0, -0.08])
        height = partial(terrain_height, terrain)
        stem = rooted_stem(plan, domain, height, position, 0.62, 1.6, oak=True)

        vertices, faces = stem["vertices"], stem["faces"]
        collar = vertices[faces[faces[:, 0] == len(vertices) - 2, 1]]
        ground = terrain_height(terrain, collar[:, :2])
        self.assertTrue(domain.covers(shapely.Polygon(collar[:, :2])))
        np.testing.assert_allclose(collar[:, 2] - ground, -0.08, atol=1e-12)
        self.assertGreater(np.linalg.norm(collar[:, :2], axis=1).max(), 0.5)

        edges = np.sort(faces[:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2), axis=1)
        _, counts = np.unique(edges, axis=0, return_counts=True)
        np.testing.assert_array_equal(counts, 2)

    def test_near_and_instanced_stems_enclose_the_same_first_branch(self):
        position = np.array([1.0, -2.0, -0.08])
        for species in ("oak", "beech"):
            for form in (0, 1):
                with self.subTest(species=species, form=form):
                    plan = stand_plan(species, form)
                    oak = species == "oak"
                    near = (
                        rooted_stem(
                            plan,
                            shapely.box(-5, -5, 5, 5),
                            lambda xy: np.zeros(len(xy)),
                            position,
                            0,
                            1,
                            oak=oak,
                        )["vertices"]
                        - position
                    )
                    template = template_stem(plan, oak=oak)["vertices"]
                    stem, branch = plan.axis(()), plan.axis((0,))
                    _, direction, _, _ = arc_frame(
                        stem.length, stem.bend, 0, branch.parent_parameter
                    )
                    tangent = stem.frame @ direction
                    radius, _ = plan.radii((0,))
                    for vertices in (near, template):
                        drift = lean_tree(plan, vertices, oak=oak) - vertices
                        from_union = vertices - drift - branch.origin
                        union = from_union[abs(from_union @ tangent) < 1e-8]
                        self.assertGreaterEqual(len(union), 8)
                        self.assertGreater(np.linalg.norm(union, axis=1).min(), radius)
                    shifted = lean_tree(plan, branch.origin, oak=oak) - branch.origin
                    self.assertGreater(np.linalg.norm(shifted), radius)
                    np.testing.assert_allclose(near[-1], template[-1], atol=1e-12)
