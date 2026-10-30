"""Whole supported caps, holes and bounded failure of the area-floor recipe."""

import unittest
from dataclasses import replace
from types import SimpleNamespace

import numpy as np
import shapely
from foot_ramp_geometry import close_caps
from ground_area_bounds import verify_geometry
from ground_area_inputs import load
from ground_area_recipe import AreaRecipe, construct, prepare
from outshine_ref.contracts import Mesh
from outshine_ref.transport.canonical_ports._affine import AffineGround
from surface_solid_checks import audit_closed


def terrain(grade=(0.01, 0.015)):
    xy = np.array([[-10, -10], [10, -10], [10, 10], [-10, 10]], float)
    return Mesh(np.c_[xy, 100 + xy @ grade], np.array([[0, 1, 2], [0, 2, 3]], "u4"))


class AreaRecipeChecks(unittest.TestCase):
    def test_actual_whole_area_fits_both_controls_and_exact_stored_bounds(self):
        inputs = load()
        prepared = prepare(inputs.original, inputs.surface, inputs.recipe)
        with self.assertRaisesRegex(ValueError, "Whole terrain relief"):
            construct(prepared, inputs.recipe, "facet")
        for method in ("affine", "highs"):
            body, upper, lower, _plane = construct(prepared, inputs.recipe, method)
            result = verify_geometry(
                body, upper, lower, inputs.surface, inputs.original, inputs.recipe
            )
            self.assertTrue(result["whole_supported_floor"])
            self.assertEqual(len(result["stored_caps"]), 2)
            self.assertEqual((len(body.vertices), len(body.triangles)), (8, 12))

    def test_existing_affine_ground_wrapper_keeps_independent_grade_answer(self):
        source = SimpleNamespace(
            surface=terrain((0.3, 0.4)),
            grade_frames=np.array([np.eye(2) / 0.1]),
            face_parts=np.zeros(2, int),
        )
        fitted = AffineGround(source)
        np.testing.assert_allclose(fitted.gradient, [0.06, 0.08], rtol=0, atol=1e-14)
        self.assertAlmostEqual(fitted.intercept, 100, places=12)
        self.assertLessEqual(fitted.report["maximum_normalized_continuous_grade"], 1)
        self.assertAlmostEqual(
            fitted.report["maximum_original_prior_error_m"], 5.6, places=12
        )

    def test_reader_rejects_supported_looking_caps_detached_from_actual_body(self):
        inputs = load()
        body, upper, lower, _plane = construct(
            prepare(inputs.original, inputs.surface, inputs.recipe),
            inputs.recipe,
            "affine",
        )
        faces = body.triangles.copy()
        faces[0] = faces[0, ::-1]
        wrong = Mesh(body.vertices, faces)
        with self.assertRaises(ValueError):
            verify_geometry(
                wrong, upper, lower, inputs.surface, inputs.original, inputs.recipe
            )

    def test_independent_checker_refuses_a_closed_but_hovering_floor(self):
        source = terrain()
        original = shapely.box(-2, -2, 2, 2)
        _body, upper, lower, _plane = construct(
            prepare(original, source, AreaRecipe()), AreaRecipe(), "affine"
        )
        top = Mesh(upper.vertices + [0, 0, 0.2], upper.triangles)
        bottom = Mesh(lower.vertices + [0, 0, 0.2], lower.triangles)
        wrong = close_caps(top, bottom)
        with self.assertRaisesRegex(ValueError, "rise/embed/grade"):
            verify_geometry(wrong, top, bottom, source, original, AreaRecipe())

    def test_whole_caps_are_supported_closed_and_leave_old_mesh_unchanged(self):
        surface = terrain()
        old_v, old_f = surface.vertices.copy(), surface.triangles.copy()
        prepared = prepare(shapely.box(-2, -2, 2, 2), surface, AreaRecipe())
        for method in ("facet", "affine"):
            body, upper, lower, note = construct(prepared, AreaRecipe(), method)
            self.assertGreater(audit_closed(body), 0)
            for cast in (np.float64, np.float32):
                a = upper.vertices.astype(cast).astype(float)
                b = lower.vertices.astype(cast).astype(float)
                self.assertGreater(
                    (a[:, 2] - (100 + a[:, :2] @ [0.01, 0.015])).min(), 0.04
                )
                self.assertLess((a[:, 2] - (100 + a[:, :2] @ [0.01, 0.015])).max(), 0.1)
                self.assertLess(
                    (b[:, 2] - (100 + b[:, :2] @ [0.01, 0.015])).max(), -0.01
                )
            self.assertLess(note["terrain_relief_m"], 1e-10)
        np.testing.assert_array_equal(old_v, surface.vertices)
        np.testing.assert_array_equal(old_f, surface.triangles)

    def test_original_hole_is_not_filled_by_whole_floor_caps(self):
        original = shapely.Polygon(
            shapely.box(-3, -3, 3, 3).exterior.coords,
            [shapely.box(-1, -1, 1, 1).exterior.coords],
        )
        prepared = prepare(original, terrain(), AreaRecipe())
        body, upper, _lower, _note = construct(prepared, AreaRecipe(), "facet")
        footprint = shapely.coverage_union_all(
            shapely.polygons(upper.vertices[upper.triangles, :2])
        )
        self.assertEqual(len(footprint.interiors), 1)
        self.assertFalse(footprint.covers(shapely.Point(0, 0)))
        self.assertTrue(footprint.covers(original))
        self.assertGreater(audit_closed(body), 0)

    def test_missing_coverage_steep_relief_and_budget_do_not_return_partial_floor(self):
        recipe = AreaRecipe()
        with self.assertRaisesRegex(ValueError, "Complete original area"):
            prepare(shapely.box(9, 9, 11, 11), terrain(), recipe)
        prepared = prepare(shapely.box(-2, -2, 2, 2), terrain((0.5, 0.3)), recipe)
        for method in ("facet", "affine"):
            with self.assertRaises(ValueError):
                construct(prepared, recipe, method)
        with self.assertRaisesRegex(ValueError, "before intersection"):
            prepare(
                shapely.box(-2, -2, 2, 2),
                terrain(),
                replace(recipe, coordinate_face_pairs_most=1),
            )
        with self.assertRaisesRegex(ValueError, "Bounded held local terrain"):
            prepare(
                shapely.box(-2, -2, 2, 2),
                terrain(),
                replace(recipe, terrain_faces_most=1),
            )

    def test_bad_dimensions_unknown_method_and_unused_vertices_are_not_admitted(self):
        for change in (
            {"minimum_rise_m": 0.06},
            {"maximum_grade": float("nan")},
            {"maximum_grade": True},
            {"minimum_embed_m": -1},
        ):
            with self.assertRaises(ValueError):
                replace(AreaRecipe(), **change)
        prepared = prepare(shapely.box(-2, -2, 2, 2), terrain(), AreaRecipe())
        with self.assertRaises(ValueError):
            construct(prepared, AreaRecipe(), "unknown")
        unused = Mesh(
            np.vstack((terrain().vertices, [[0, 0, 100]])), terrain().triangles
        )
        with self.assertRaisesRegex(ValueError, "actually used"):
            prepare(shapely.box(-2, -2, 2, 2), unused, AreaRecipe())


if __name__ == "__main__":
    unittest.main()
