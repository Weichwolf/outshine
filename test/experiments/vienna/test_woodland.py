import json
import unittest
from functools import partial
from importlib.resources import files
from types import SimpleNamespace

import numpy as np
import shapely
from montane_trees import conifer_sites, needle_sprays, spruce_plan
from outshine_ref.vegetation import PlantPlan
from outshine_ref.vegetation.curve_frame import arc_frame
from scene_mesh import terrain_grid, terrain_height
from scipy.spatial import cKDTree
from woodland import (
    avenue_plan,
    crown_leaf_frames,
    represented_leaves,
    scaffold_parts,
    stand_plan,
    woodland_surface,
)
from woodland_fork import joined_wood
from woodland_leaves import blade_kite, grouped_blades
from woodland_stem import lean_tree, rooted_stem, stem_profile, template_stem


class WoodlandTest(unittest.TestCase):
    def test_conifer_selection_uses_local_elevation_and_mapped_leaf_type(self):
        xy = np.column_stack((np.arange(6), np.zeros(6)))
        source = SimpleNamespace(
            lat=47.25,
            lon=10.1,
            datum=1400,
            features=[
                (
                    "landcover",
                    shapely.box(3.5, -1, 4.5, 1),
                    {"class": "wood", "leaf_type": "broadleaved"},
                ),
                (
                    "landcover",
                    shapely.box(4.5, -1, 5.5, 1),
                    {"class": "wood", "leaf_type": "needleleaved"},
                ),
            ],
        )
        heights = np.array([-310, -15, -15, 265, 265, -310])
        rank = np.array([0.2, 0.2, 0.8, 0.8, 0.2, 0.8])
        np.testing.assert_array_equal(
            conifer_sites(source, xy, heights, rank),
            [False, True, False, True, False, True],
        )
        source.lat, source.lon = 48.2, 16.4
        np.testing.assert_array_equal(
            conifer_sites(source, xy, -1100 * np.ones(6), rank),
            [False, False, False, False, False, True],
        )

    def test_spruce_keeps_a_conical_crown_in_the_grouped_representation(self):
        for form in (0, 1):
            with self.subTest(form=form):
                plant = spruce_plan(form)
                close, near_faces = needle_sprays(plant, 1)
                coarse, far_faces = needle_sprays(plant, 32)
                self.assertLess(len(far_faces), len(near_faces) / 8)
                self.assertGreater(coarse[:, 2].max(), 0.93 * close[:, 2].max())
                height = plant.wood.axis(()).length
                lower = coarse[:, 2] < 0.5 * height
                upper = coarse[:, 2] > 0.8 * height
                reach = np.linalg.norm(coarse[:, :2], axis=1)
                self.assertLess(reach[upper].max(), 0.6 * reach[lower].max())

    def test_leaf_roll_keeps_the_seeded_blades_on_their_midribs(self):
        count = 512
        height = np.repeat((2.0, 8.0), count // 2)
        rows = {
            "leaf_origins": np.column_stack((np.zeros((count, 2)), height)),
            "leaf_frames": np.tile(np.eye(3), (count, 1, 1)),
        }
        beech = crown_leaf_frames(rows, 12, oak=False)
        oak = crown_leaf_frames(rows, 12, oak=True)
        np.testing.assert_array_equal(oak, crown_leaf_frames(rows, 12, oak=True))
        np.testing.assert_array_equal(oak[:, :, 1], rows["leaf_frames"][:, :, 1])
        np.testing.assert_allclose(np.linalg.det(oak), 1)
        beech_tilt, oak_tilt = (np.arccos(r[:, 2, 2]) for r in (beech, oak))
        self.assertGreater(oak_tilt.mean(), beech_tilt.mean())
        self.assertGreater(oak_tilt[count // 2 :].mean(), oak_tilt[: count // 2].mean())

    def test_middle_canopy_samples_both_sides_of_an_alternating_shoot(self):
        rows = {
            "leaf_axis_ids": np.zeros(64, dtype=int),
            "leaf_variants": np.zeros(64, dtype=int),
            "leaf_origins": np.zeros((64, 3)),
        }
        leaves, _, _ = represented_leaves(rows, np.ones(1), 4)
        np.testing.assert_array_equal(leaves // 4, np.arange(16))
        self.assertEqual(set(leaves % 2), {0, 1})

    def test_distant_canopy_preserves_leaf_area_with_fewer_representatives(self):
        plan = stand_plan("oak", 0)
        rows = plan.tables(sum(plan.axis_counts), sum(plan.leaf_counts))
        areas = np.array([blade.area() for blade in plan.blades])
        variants = rows["leaf_variants"]
        frames = crown_leaf_frames(rows, plan.seed, oak=True)
        _, middle = grouped_blades(
            rows, plan.blades, frames, areas, 16, parent_levels=0
        )
        vertices, far = grouped_blades(
            rows, plan.blades, frames, areas, 32, parent_levels=1
        )
        self.assertLess(len(far), len(middle))
        corners = vertices[far]
        area = (
            0.5
            * np.linalg.norm(
                np.cross(corners[:, 1] - corners[:, 0], corners[:, 2] - corners[:, 0]),
                axis=1,
            ).sum()
        )
        self.assertAlmostEqual(float(area), float(areas[variants].sum()))

    def test_grouped_leaves_preserve_area_and_separated_shoot_positions(self):
        blade = stand_plan("oak", 0).blades[0]
        count = 9
        x = np.array([-0.37, -0.35, -0.33, -0.31, 0.31, 0.33, 0.35, 0.37, 0.8])
        origins = np.column_stack((x, np.zeros((count, 2))))
        rows = {
            "leaf_axis_ids": np.zeros(count, dtype=int),
            "leaf_variants": np.zeros(count, dtype=int),
            "leaf_origins": origins,
        }
        frames = np.tile(np.eye(3), (count, 1, 1))
        area = blade.area()
        vertices, faces = grouped_blades(
            rows, (blade,), frames, np.array([area]), 8, parent_levels=0
        )
        self.assertEqual(len(faces), 6)
        corners = vertices[faces]
        triangle_areas = 0.5 * np.linalg.norm(
            np.cross(corners[:, 1] - corners[:, 0], corners[:, 2] - corners[:, 0]),
            axis=1,
        )
        self.assertAlmostEqual(float(triangle_areas.sum()), count * area)
        areas = triangle_areas.reshape(-1, 2)
        centres = np.sum(
            areas[..., None] * corners.mean(axis=1).reshape(-1, 2, 3), axis=1
        )
        centres /= areas.sum(axis=1)[:, None]
        _, _, offset = blade_kite(blade)
        np.testing.assert_allclose(
            np.sort(centres[:, 0]), np.array([-0.34, 0.34, 0.8]) + offset[0]
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

    def test_broadleaf_forks_share_one_closed_shell_and_retain_root_shape(self):
        from manifold3d import Error, Manifold, Mesh64

        for species, plan in (
            ("oak", stand_plan("oak", 0)),
            ("chestnut", avenue_plan(1)),
        ):
            with self.subTest(species=species):
                oak = species == "oak"

                def height(xy):
                    return 0.50 * (xy[:, 0] - 5) + 0.20 * xy[:, 1]

                position = np.array([5.0, 0.0, -0.08])
                angle, scale = 0.69, 1.3
                stem = rooted_stem(
                    plan,
                    shapely.box(3, -3, 8, 3),
                    height,
                    position,
                    angle,
                    scale,
                    oak=oak,
                    seed=7291,
                )
                limbs = tuple(scaffold_parts(plan, 4, species))
                vertices, faces = joined_wood(
                    stem, limbs, position, angle, scale, plan.radii(())[0]
                )
                shell = Manifold(Mesh64(vertices.copy(), faces.copy()))
                self.assertEqual(shell.status(), Error.NoError)
                self.assertEqual(len(shell.decompose()), 1)
                root = stem["vertices"][
                    stem["vertices"][:, 2] - height(stem["vertices"][:, :2]) < 0
                ]
                self.assertGreater(len(root), 48)
                np.testing.assert_allclose(
                    cKDTree(vertices).query(root)[0], 0, atol=1e-10
                )

    def test_near_and_instanced_stems_enclose_the_same_first_branch(self):
        position = np.array([1.0, -2.0, -0.08])
        for species in ("oak", "beech", "chestnut"):
            for form in (0, 1):
                with self.subTest(species=species, form=form):
                    plan = (
                        avenue_plan(form)
                        if species == "chestnut"
                        else stand_plan(species, form)
                    )
                    oak = species == "oak"
                    rooted = rooted_stem(
                        plan,
                        shapely.box(-5, -5, 5, 5),
                        lambda xy: np.zeros(len(xy)),
                        position,
                        0,
                        1,
                        oak=oak,
                        seed=9217,
                    )
                    instance = template_stem(plan, oak=oak)
                    near, template = rooted["vertices"] - position, instance["vertices"]
                    stem, branch = plan.axis(()), plan.axis((0,))
                    _, direction, lateral, normal = arc_frame(
                        stem.length, stem.bend, 0, branch.parent_parameter
                    )
                    tangent = stem.frame @ direction
                    section_frame = stem.frame @ np.column_stack((lateral, normal))
                    radius, _ = plan.radii((0,))
                    for vertices, mesh in ((near, rooted), (template, instance)):
                        drift = lean_tree(plan, vertices, oak=oak) - vertices
                        from_union = vertices - drift - branch.origin
                        axial = from_union @ tangent
                        edges = mesh["faces"][:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2)
                        left, right = edges[
                            (axial[edges[:, 0]] * axial[edges[:, 1]]) < 0
                        ].T
                        weight = axial[left] / (axial[left] - axial[right])
                        cut = from_union[left] + weight[:, None] * (
                            from_union[right] - from_union[left]
                        )
                        cut = np.vstack((cut, from_union[abs(axial) < 1e-8]))
                        points = np.unique(np.round(cut @ section_frame, 8), axis=0)
                        angles = np.arctan2(points[:, 1], points[:, 0])
                        surface = shapely.Polygon(points[np.argsort(angles)])
                        self.assertTrue(surface.contains(shapely.Point(0, 0)))
                        self.assertGreater(
                            surface.boundary.distance(shapely.Point(0, 0)), radius
                        )
                    np.testing.assert_allclose(near[-1], template[-1], atol=1e-12)

    def test_roots_form_low_separate_ridges(self):
        for species in ("oak", "beech"):
            with self.subTest(species=species):
                plan = stand_plan(species, 0)
                radius, _ = plan.radii(())
                t, _, _, _, _, flare = stem_profile(
                    plan, 1, oak=species == "oak", grounded=True, seed=9217
                )
                high = t * plan.axis(()).length >= radius
                self.assertGreater(flare[0].max(), radius)
                self.assertLess(np.mean(flare[0] > 0.5 * radius), 0.30)
                self.assertLess(flare[high].max(), 0.30 * radius)

    def test_root_ridge_keeps_ground_contact_on_a_steep_grade(self):
        def height(points):
            return 0.8 * (points[:, 0] + 0.2 * points[:, 1])

        stem = rooted_stem(
            stand_plan("oak", 1),
            shapely.box(-3, -3, 1, 3),
            height,
            np.array([0, 0, -0.08]),
            15,
            1.1,
            oak=True,
            seed=30,
        )
        vertices, faces = stem["vertices"], stem["faces"]
        collar = vertices[faces[faces[:, 0] == len(vertices) - 2, 1]]
        np.testing.assert_allclose(collar[:, 2], height(collar[:, :2]) - 0.08)
