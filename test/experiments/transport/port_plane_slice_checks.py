"""Isoline cells retain the complete XY domain, source anchors and whole ports."""

import io
import time
import unittest
from argparse import Namespace
from contextlib import redirect_stdout

import numpy as np
import shapely
from port_plane_adaptive import quality, resample_road
from port_plane_checks import field
from port_plane_layout import prepare_plane_field
from port_plane_slice_experiment import solve_slices
from port_plane_slice_fit import fit_sliced
from port_plane_slices import SliceBuilder, initial_knots, slice_mesh, weld_vertices
from port_plane_system import continuous_matrix, vertex_matrices
from port_plane_world_checks import strip_and_soil


class SliceChecks(unittest.TestCase):
    def test_complete_fit_starts_without_external_generated_seed(self):
        product, original = strip_and_soil()
        model, _ = prepare_plane_field(product)
        captured = {
            "ground_targets": original["targets"],
            "road_frames": original["frames"][:6],
            "road_vertex_map": np.arange(8),
        }
        seed = {
            "plane_coefficients": np.zeros(3 * len(model.origins)),
            "vertices": np.c_[original["xy"], np.full(len(original["xy"]), 7)],
            "datum_m": 7,
        }
        options = Namespace(fit=True, seconds=1, method="cone", segments=8, rounds=3)
        with redirect_stdout(io.StringIO()):
            mesh, _, _, marked, _, records = solve_slices(
                options, model, product, captured, original, seed, time.monotonic()
            )
        self.assertFalse(marked.any())
        self.assertTrue(records[-1]["quality"]["whole_world_valid"])
        self.assertTrue(records[0]["fitting"]["solver"]["valid"])
        np.testing.assert_allclose(
            mesh["heights"][mesh["original_vertex_map"]],
            original["targets"],
            atol=1e-7,
            rtol=0,
        )

    def test_rounding_snap_retains_the_actual_original_corner_support(self):
        model = field()
        xy = np.array([[71.46456496828081, -2], [91.6547313, 0], [100, -2]])
        builder = SliceBuilder(model, xy, np.zeros(3))
        knots = np.array([0, xy[0, 0] / 100 - 5e-16, 0.75, 1])
        faces, _ = builder.triangle(np.array([0, 2, 1]), 0, knots)
        shapes = shapely.polygons(np.asarray(builder.xy)[faces])
        self.assertAlmostEqual(shapely.union_all(shapes).area, shapely.Polygon(xy).area)
        self.assertLessEqual(len(builder.xy), 7)

    def test_rounding_weld_preserves_resolvable_gaps_and_distinct_port_corners(self):
        xy = np.array([[0, 0], [1e-8, 0], [1e-12, 0], [2e-12, 0]])
        labels, note = weld_vertices(xy, np.array([True, True, False, True]), 2, 1e-11)
        self.assertEqual(labels[0], 0)
        self.assertEqual(labels[1], 1)
        self.assertEqual(labels[3], 3)
        self.assertEqual(labels[2], 0)
        self.assertGreater(note["distinct_protected_vertices_kept"], 0)

    def test_joint_fit_uses_the_original_observations_and_audits_complete_faces(self):
        product, original = strip_and_soil()
        model, _ = prepare_plane_field(product)
        mesh, _ = slice_mesh(
            model,
            original,
            product,
            {"road_vertex_map": np.arange(8)},
            initial_knots(model, original, 8),
        )
        certificates = continuous_matrix(
            model,
            product["vertices"],
            product["indices"],
            product["face_owners"],
            original["frames"][:6],
        )
        result, system, solver = fit_sliced(
            model,
            {"ground_targets": original["targets"]},
            mesh,
            certificates,
            seconds=1,
        )
        self.assertIsNotNone(result, solver)
        self.assertEqual(system["objective_observations"], len(original["xy"]))
        self.assertGreater(len(mesh["xy"]), system["objective_observations"])
        seed, _coefficients = result
        marked, audit, _ = quality(
            model, seed["plane_coefficients"], mesh, product["part_roles"]
        )
        self.assertFalse(marked.any(), audit)
        self.assertTrue(audit["whole_world_valid"], audit)
        np.testing.assert_allclose(
            mesh["heights"][mesh["original_vertex_map"]],
            original["targets"],
            atol=1e-7,
            rtol=0,
        )
        failed, _, budget = fit_sliced(
            model,
            {"ground_targets": original["targets"]},
            mesh,
            certificates,
            seconds=0,
        )
        self.assertIsNone(failed)
        self.assertFalse(budget["valid"])

    def test_two_parameter_rows_preserve_the_whole_joint_domain(self):
        product, original = strip_and_soil()
        model, _ = prepare_plane_field(product)
        captured = {"road_vertex_map": np.arange(8)}
        knots = initial_knots(model, original, 16)
        mesh, note = slice_mesh(model, original, product, captured, knots)
        self.assertLessEqual(note["numeric_weld_m"], note["weld_tolerance_m"])
        original_shape = shapely.coverage_union_all(
            shapely.polygons(original["xy"][original["faces"]])
        )
        generated = shapely.polygons(mesh["xy"][mesh["faces"]])
        self.assertTrue(shapely.coverage_is_valid(generated))
        self.assertLess(
            shapely.symmetric_difference(
                shapely.coverage_union_all(generated), original_shape
            ).area,
            1e-10,
        )
        np.testing.assert_array_equal(
            mesh["xy"][mesh["original_vertex_map"]], original["xy"]
        )
        self.assertEqual(len(mesh["port_roots"]), 2)
        for edge in original["port_roots"]:
            np.testing.assert_array_equal(
                mesh["xy"][mesh["original_vertex_map"][list(edge)]],
                original["xy"][list(edge)],
            )

    def test_curved_height_rows_meet_grade_height_and_full_port_bounds(self):
        product, original = strip_and_soil()
        model, _ = prepare_plane_field(product)
        mesh, _ = slice_mesh(
            model,
            original,
            product,
            {"road_vertex_map": np.arange(8)},
            initial_knots(model, original, 32),
        )
        coefficients = np.array([-0.1, 0.02, 0.012, 2.95, -0.01, 0.012])
        h, _, _, parts = vertex_matrices(
            model, mesh["xy"], mesh["faces"], mesh["parts"]
        )
        soil = np.flatnonzero(parts < 0)
        x, y = mesh["xy"][soil].T
        soil_z = np.where(x <= 0, 0.02 * x, 3 - 0.01 * (x - 100)) + 0.012 * y
        mesh["heights"] = h @ np.r_[coefficients, soil_z]
        resample_road(model, coefficients, 0, mesh)
        marked, audit, _ = quality(model, coefficients, mesh, product["part_roles"])
        self.assertFalse(marked.any(), audit)
        self.assertTrue(audit["whole_world_valid"], audit)
        self.assertTrue(audit["geometry_complete"], audit)


if __name__ == "__main__":
    unittest.main()
