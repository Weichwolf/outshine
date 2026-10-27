"""Containment precision, physical refusal and original-mask diagnostics."""

import unittest

import numpy as np
import shapely
from compare_canonical_water_domains import DEFAULT
from compare_water_grid_height import inputs
from shapely.affinity import translate
from water_nodal_height import cell_faces, parameter_triangle, sample_face
from water_parameter_cut import cut_cell

CELL = (14, 8628, 5745)


def root(keys):
    xy = parameter_triangle(keys) - CELL[1:]
    return "admitted", np.c_[xy * 100, 500 + xy[:, 0] - xy[:, 1]]


def cook(polygons, **options):
    return cut_cell(
        CELL,
        polygons,
        tuple(range(len(polygons))),
        root,
        context="analytic-v1",
        **options,
    )[0]


def global_polygon(p):
    return translate(p, xoff=CELL[1], yoff=CELL[2])


class LocalParameterSampling(unittest.TestCase):
    def test_origin_keeps_world_plane_and_original_guard(self):
        keys = cell_faces(CELL)[0]
        _, world = root(keys)
        local = parameter_triangle(keys) - CELL[1:]
        np.testing.assert_array_equal(
            sample_face(keys, world, local, origin=CELL[1:]), world
        )
        np.testing.assert_array_equal(sample_face(keys, world, local + CELL[1:]), world)
        with self.assertRaisesRegex(ValueError, "outside canonical facet"):
            sample_face(keys, world, [[0.1, 0.2]], origin=CELL[1:])
        for origin in ([np.nan, 0], [0.5, 0], [0], [[0, 0]]):
            with self.assertRaisesRegex(ValueError, "integer Mercator origin"):
                sample_face(keys, world, local, origin=origin)

    def test_original_geos_global_intersections_refuse_but_local_cuts_pass(self):
        _, records, _, _ = inputs(DEFAULT)
        polygons = [r[1].polygon for r in records if r[0] == CELL]
        mask = shapely.union_all(polygons)
        refusals = 0
        for keys in cell_faces(CELL):
            cut = shapely.Polygon(parameter_triangle(keys)).intersection(mask)
            for part in shapely.get_parts(cut):
                if part.geom_type != "Polygon" or part.area <= 0:
                    continue
                for triangle in shapely.get_parts(
                    shapely.constrained_delaunay_triangles(part)
                ):
                    try:
                        sample_face(
                            keys,
                            root(keys)[1],
                            np.asarray(triangle.exterior.coords)[:3],
                        )
                    except ValueError as error:
                        self.assertIn("outside canonical facet", str(error))
                        refusals += 1
        self.assertGreater(refusals, 0)
        product = cook(polygons)
        self.assertGreater(len(product.world), 0)
        self.assertEqual(product.report["state"], "diagnostic_cut")


class ParameterCuts(unittest.TestCase):
    def setUp(self):
        island = shapely.box(0.0625, 0.0625, 0.1875, 0.1875)
        ring = shapely.Polygon(
            [(0.25, 0.125), (0.875, 0.125), (0.875, 0.875), (0.25, 0.875)],
            [[(0.5, 0.25), (0.75, 0.25), (0.75, 0.5), (0.5, 0.5)]],
        )
        self.polygons = [global_polygon(island), global_polygon(ring)]

    def test_islands_holes_parent_normals_and_original_source_membership(self):
        product = cook(self.polygons)
        local = product.parameter
        np.testing.assert_allclose(
            product.world[:, :, :2], local * 100, rtol=0, atol=1e-13
        )
        np.testing.assert_allclose(
            product.world[:, :, 2],
            500 + local[:, :, 0] - local[:, :, 1],
            rtol=0,
            atol=1e-12,
        )
        self.assertEqual(set(product.membership.members), {0, 1})
        self.assertEqual(product.source_keys, (0, 1))
        for parent in set(product.parents):
            _, vertices = root(cell_faces(CELL)[parent])
            normal = np.cross(vertices[1] - vertices[0], vertices[2] - vertices[0])
            expected = np.tile(
                normal / np.linalg.norm(normal), ((product.parents == parent).sum(), 1)
            )
            np.testing.assert_array_equal(
                product.normals[product.parents == parent], expected
            )
        for key in ("missing", "outside", "overlap", "source_uncovered"):
            self.assertLessEqual(
                product.report["coverage"][key], product.report["coverage"]["tolerance"]
            )

    def test_refinement_preserves_boundary_points_and_triples_faces(self):
        coarse, fine = cook(self.polygons), cook(self.polygons, refine_interior=True)
        self.assertEqual(len(fine.world), 3 * len(coarse.world))
        for point, world in zip(
            coarse.parameter.reshape(-1, 2), coarse.world.reshape(-1, 3)
        ):
            matches = np.all(fine.parameter == point, axis=2)
            self.assertTrue(matches.any())
            np.testing.assert_array_equal(
                fine.world[matches], np.tile(world, (matches.sum(), 1))
            )

    def test_missing_or_rejected_parent_publishes_no_partial_cell(self):
        for state in ("pending_sources", "physical_rejection", "budget_exhausted"):
            calls = []

            def missing(keys, calls=calls, state=state):
                calls.append(keys)
                return root(keys) if len(calls) == 1 else (state, None)

            product, report = cut_cell(
                CELL, self.polygons, (0, 1), missing, context="v1"
            )
            self.assertIsNone(product)
            self.assertEqual(report["state"], state)
            self.assertEqual(len(calls), 2)

    def test_invalid_source_parent_context_and_budget_refuse(self):
        with self.assertRaisesRegex(ValueError, "inside their canonical cell"):
            cook([global_polygon(shapely.box(-0.1, 0, 0.5, 0.5))])
        with self.assertRaisesRegex(ValueError, "face budget"):
            cook(self.polygons, face_limit=1)
        with self.assertRaisesRegex(ValueError, "valid source"):
            cook(self.polygons, coordinate_limit=1)
        with self.assertRaisesRegex(ValueError, "context"):
            cut_cell(CELL, self.polygons, (0, 1), root, context="")
        with self.assertRaisesRegex(ValueError, "nondegenerate"):
            cut_cell(
                CELL,
                self.polygons,
                (0, 1),
                lambda _: ("admitted", np.zeros((3, 3))),
                context="v1",
            )


if __name__ == "__main__":
    unittest.main()
