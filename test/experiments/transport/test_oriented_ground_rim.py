"""Whole source-bound outputs and independently known rectangle counterexamples."""

import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from ground_contact import ContactLimits
from ground_contact_checks import qualify_contact
from ground_contact_inputs import load_context
from ground_interior import relax_contact
from ground_rim_contact import build_rim_contact
from ground_rim_domain import rim_rectangle
from ground_rim_sampling import RimRecipe
from ground_rim_validation import compare_domains, domain_grid, validation_points
from outshine_ref.contracts import Mesh


class RectangleDomain(unittest.TestCase):
    def test_known_rotated_rectangle_keeps_margin_without_axis_area_waste(self):
        angle = np.deg2rad(37)
        axes = np.array(
            [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
        )
        xy = np.array([[0, 0], [40, 0], [40, 2], [0, 2]]) @ axes.T + [12, -7]
        domain = rim_rectangle(xy, 5, "oriented")
        self.assertTrue(domain.contains(shapely.MultiPoint(xy)))
        self.assertAlmostEqual(domain.area, 50 * 12, places=10)
        self.assertAlmostEqual(
            shapely.distance(domain.boundary, shapely.MultiPoint(xy)), 5, places=10
        )
        self.assertLess(domain.area, 0.6 * rim_rectangle(xy, 5, "axis").area)

    def test_source_order_does_not_change_the_declared_domain(self):
        xy = np.array([[0, 0], [25, 9], [28, 12], [2, 3], [7, 3]])
        original = rim_rectangle(xy, 5, "oriented")
        for order in (xy[::-1], np.roll(xy, 2, axis=0)):
            self.assertTrue(
                original.equals_exact(rim_rectangle(order, 5, "oriented"), 0)
            )

    def test_degenerate_domain_and_unknown_recipe_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "Nondegenerate"):
            rim_rectangle(np.array([[0.0, 0.0], [1.0, 1.0]]), 5, "oriented")
        with self.assertRaises(ValueError):
            RimRecipe(boundary="unknown")
        with self.assertRaises(ValueError):
            rim_rectangle(np.array([[0.0, 0.0], [1.0, 1.0]]), 5, "unknown")


class ActualOrientedRim(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = []
        for options in ({}, {"seed": (246935327359419868, 0), "geometry_recipe": None}):
            context = load_context(**options)
            outputs = {}
            for boundary in ("axis", "oriented"):
                base = build_rim_contact(
                    context.road,
                    context.road_dem_z,
                    context.sample_z,
                    limits=ContactLimits(margins_m=(15.0,)),
                    recipe=RimRecipe(boundary=boundary),
                )
                outputs[boundary] = relax_contact(context.road, base, context.sample_z)
            cls.cases.append((context, outputs))

    def test_two_real_components_keep_whole_roads_soil_profiles_and_capacity(self):
        for context, outputs in self.cases:
            for result in outputs.values():
                quality = qualify_contact(context.road, result)
                self.assertEqual(
                    quality["road_vertices_exact"], len(context.road.vertices)
                )
                self.assertEqual(
                    quality["road_faces_exact"], len(context.road.triangles)
                )
                self.assertLessEqual(quality["maximum_soil_grade"], (2 / 3) * 1.0000001)
                self.assertEqual(quality["outer_edges"], 64)
                corners = result["rim_profile"]["corner_vertex_ids"]
                actual = result["mesh"].vertices[corners]
                np.testing.assert_array_equal(
                    actual[:, 2], context.sample_z(actual[:, :2])
                )
                self.assertLessEqual(
                    result["rim_profile"]["maximum_observation_adjustment_m"], 2
                )
            self.assertEqual(
                len(outputs["axis"]["mesh"].vertices),
                len(outputs["oriented"]["mesh"].vertices),
            )
            self.assertLess(
                outputs["oriented"]["domain"].area,
                0.8 * outputs["axis"]["domain"].area,
            )

    def test_explicit_recipe_preserves_the_default_profile_contract(self):
        self.assertEqual(RimRecipe().version, "outshine/local-fitted-terrain-rim/1")
        self.assertNotEqual(RimRecipe().version, RimRecipe(boundary="oriented").version)
        for _, outputs in self.cases:
            self.assertNotIn("boundary", outputs["axis"]["rim_profile"]["recipe"])
            self.assertNotIn("rectangle_corners_xy", outputs["axis"]["rim_profile"])
            self.assertEqual(
                outputs["oriented"]["rim_profile"]["recipe"]["boundary"], "oriented"
            )

    def test_changed_or_missing_declared_corners_do_not_pass(self):
        context, outputs = self.cases[0]
        result = outputs["oriented"]
        profile = dict(result["rim_profile"])
        del profile["rectangle_corners_xy"]
        with self.assertRaisesRegex(ValueError, "corners_missing"):
            qualify_contact(context.road, result | {"rim_profile": profile})
        profile["rectangle_corners_xy"] = result["rim_profile"][
            "rectangle_corners_xy"
        ].copy()
        profile["rectangle_corners_xy"][0, 0] += 0.01
        with self.assertRaisesRegex(ValueError, "dem_corner_changed"):
            qualify_contact(context.road, result | {"rim_profile": profile})

    def test_rotated_perimeter_controls_follow_the_actual_boundary(self):
        for _, outputs in self.cases:
            result = outputs["oriented"]
            _, xy = validation_points(result)
            distance = shapely.distance(shapely.points(xy), result["domain"].boundary)
            self.assertLess(float(max(distance)), 1e-10)

    def test_adaptive_boundary_passes_whole_guards_or_explicitly_stays_pending(self):
        context, _ = self.cases[0]
        result = build_rim_contact(
            context.road,
            context.road_dem_z,
            context.sample_z,
            limits=ContactLimits(margins_m=(15.0,)),
            recipe=RimRecipe(boundary="oriented", sampling="adaptive"),
        )
        if result["status"] == "candidate":
            quality = qualify_contact(context.road, result)
            self.assertLessEqual(quality["outer_edges"], 64)
            self.assertEqual(quality["whole_road_soil_edges"], 90)
        else:
            self.assertEqual(result["status"], "pending")
            self.assertEqual(result["reason"], "no_qualified_rim_contact_variant")
            self.assertNotIn("mesh", result)
            self.assertTrue(
                all(row["status"] == "pending" for row in result["candidates"])
            )

    def test_actual_bridge_tops_remain_unburied_in_their_own_original_frame(self):
        from ground_interior_validation import bridge_composition

        report, _ = bridge_composition(boundary="oriented")
        self.assertEqual(len(report["bridges"]), 2)
        for row in report["bridges"]:
            self.assertGreaterEqual(row["soil_to_top"]["minimum_clearance_m"], -1e-8)


class CommonDomainControls(unittest.TestCase):
    def test_narrow_diagonal_keeps_known_controls_without_allocating_empty_box(self):
        domain = shapely.Polygon([[0, 0], [100, 100], [101, 99], [1, -1]])
        xy, tested = domain_grid({"axis": domain, "oriented": domain}, 1.0, 1000)
        expected = {(float(x), float(x - 1)) for x in range(1, 101)}
        self.assertEqual(set(map(tuple, xy)), expected)
        self.assertEqual(len(xy), 100)
        self.assertLess(tested, 1000)

    @staticmethod
    def surface(xy):
        xy = np.asarray(xy, dtype=float)
        return Mesh(
            np.c_[xy, xy[:, 0] + 2 * xy[:, 1]], np.array([[0, 1, 2], [0, 2, 3]])
        )

    def test_independent_known_plane_on_different_domains_has_no_error(self):
        meshes = {
            "axis": self.surface([[0, 0], [4, 0], [4, 4], [0, 4]]),
            "oriented": self.surface([[2, -1], [5, 2], [2, 5], [-1, 2]]),
        }
        road = Mesh(
            np.array([[1, 1, 3], [2, 1, 4], [1, 2, 5]], dtype=float),
            np.array([[0, 1, 2]]),
        )
        context = SimpleNamespace(
            road=road, sample_z=lambda xy: xy[:, 0] + 2 * xy[:, 1]
        )
        results = {
            name: {"mesh": mesh, "domain": shapely.Polygon(mesh.vertices[:, :2])}
            for name, mesh in meshes.items()
        }
        report, _ = compare_domains(context, results)
        for metrics in report["methods"].values():
            self.assertGreater(metrics["same_common_soil"]["points"], 10)
            self.assertLess(metrics["same_common_soil"]["maximum_abs_m"], 1e-12)
        with self.assertRaisesRegex(ValueError, "grid budget"):
            compare_domains(context, results, maximum_grid_points=3)


if __name__ == "__main__":
    unittest.main()
