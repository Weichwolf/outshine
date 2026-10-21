import unittest

import numpy as np
import shapely
from shapely.affinity import translate
from water_bed_distance import BankDistance
from water_bed_fit import fitted_bed
from water_bed_mesh import build_bed, refine_bed
from water_indexed_cell import index_cell
from water_nodal_height import parameter_triangle
from water_parameter_cut import cut_cell


def water_patch():
    cell = (14, 8192, 8192)
    local = shapely.box(0.125, 0.125, 0.875, 0.875).difference(
        shapely.box(0.3, 0.3, 0.4, 0.4)
    )
    polygons = [translate(local, xoff=cell[1], yoff=cell[2])]

    def facet(keys):
        xy = parameter_triangle(keys) - cell[1:]
        return "admitted", np.c_[
            6378137 + 10 + xy[:, 0], xy[:, 0] * 100, -xy[:, 1] * 100
        ]

    cut, _report = cut_cell(
        cell, polygons, ("analytic-water",), facet, context="bed-test"
    )
    return index_cell(cut, polygons)


class BedMeshControls(unittest.TestCase):
    def setUp(self):
        self.water = water_patch()
        self.layout = refine_bed(self.water, self.water.boundary)
        self.depth = np.full(len(self.layout.top_vertices), 2.0)
        self.depth[self.layout.bank_vertices] = 0
        self.up = np.tile([1.0, 0.0, 0.0], (len(self.depth), 1))

    def test_inner_points_create_a_bed_with_fixed_shores_and_original_face_lineage(
        self,
    ):
        bed = build_bed(self.layout, self.depth, self.up)
        self.assertIs(bed.water, self.water)
        self.assertEqual(len(bed.faces), 6 * len(self.water.faces))
        np.testing.assert_array_equal(np.bincount(bed.parent_faces), 6)
        np.testing.assert_array_equal(
            bed.vertices[self.layout.bank_vertices],
            self.layout.top_vertices[self.layout.bank_vertices],
        )
        self.assertTrue((bed.vertices[:, 0] < self.layout.top_vertices[:, 0]).any())
        self.assertLessEqual(
            bed.report["maximum_water_plane_penetration_m"],
            bed.report["coordinate_roundoff_allowance_m"],
        )
        self.assertEqual(bed.report["source_csr_copies"], 0)
        self.assertFalse(bed.vertices.flags.writeable)
        self.assertFalse(bed.faces.flags.writeable)

    def test_refinement_is_conforming_and_shares_every_internal_edge(self):
        directed = np.concatenate(
            [
                self.layout.faces[:, [0, 1]],
                self.layout.faces[:, [1, 2]],
                self.layout.faces[:, [2, 0]],
            ]
        )
        edges, counts = np.unique(np.sort(directed, axis=1), axis=0, return_counts=True)
        self.assertTrue(np.isin(counts, [1, 2]).all())
        np.testing.assert_array_equal(
            edges[counts == 1],
            np.sort(self.layout.boundary, axis=1)[
                np.lexsort(np.sort(self.layout.boundary, axis=1).T[::-1])
            ],
        )
        self.assertEqual(len(self.layout.boundary), 2 * len(self.water.boundary))

    def test_physical_fit_keeps_all_boundary_positions_and_stays_below_water(self):
        initial = build_bed(self.layout, self.depth, self.up)
        bed = fitted_bed(
            self.layout,
            initial,
            directional_grade=0.7,
            max_depth_m=3,
            seconds=5,
            grade_reserve=0.01,
        )
        fixed = np.unique(bed.boundary)
        np.testing.assert_array_equal(bed.vertices[fixed], initial.vertices[fixed])
        self.assertLessEqual(bed.report["materialized_grade_excess"], 1e-9)
        self.assertLessEqual(bed.report["materialized_vertex_bound_excess_m"], 1e-9)

    def test_invalid_roles_depths_or_up_do_not_publish_a_bed(self):
        with self.assertRaises(ValueError):
            refine_bed(self.water, [[0, len(self.water.world) + 1]])
        with self.assertRaises(ValueError):
            refine_bed(self.water, [[0.0, 1.0]])
        for bad in (-1, float("nan"), 1e100):
            depth = self.depth.copy()
            depth[-1] = bad
            with self.assertRaises(ValueError):
                build_bed(self.layout, depth, self.up)
        depth = self.depth.copy()
        depth[self.layout.bank_vertices[0]] = 1
        with self.assertRaises(ValueError):
            build_bed(self.layout, depth, self.up)
        with self.assertRaises(ValueError):
            build_bed(self.layout, self.depth, -self.up)
        with self.assertRaises(ValueError):
            build_bed(self.layout, self.depth, self.up * 2)


class BankDistanceControls(unittest.TestCase):
    def test_long_segments_and_random_near_segments_match_the_full_scan(self):
        rng = np.random.default_rng(20261021)
        segments = rng.normal(size=(30, 2, 3)) * 50
        segments = np.concatenate([segments, np.array([[[-100, 0, 0], [100, 0, 0]]])])
        segments[:, :, 0] += 6378137
        points = 0.2 * segments[:, 0] + 0.8 * segments[:, 1] + [0, 0.2, 0.1]
        points = np.vstack([points, [6378137 + 100, 0.25, 0]])
        bank = BankDistance(segments)
        scan, _ = bank.query(points, cap_m=4, method="scan")
        indexed, _ = bank.query(points, cap_m=4, method="index")
        np.testing.assert_array_equal(scan, indexed)
        self.assertEqual(indexed[-1], 0.25)

    def test_budget_refusal_does_not_modify_inputs_or_return_partial_distances(self):
        segments = np.array([[[0, 0, 0], [1, 0, 0]]], float)
        before = segments.copy()
        bank = BankDistance(segments)
        for method in ("scan", "index"):
            with self.assertRaisesRegex(ValueError, "budget"):
                bank.query(
                    np.array([[0.5, 0.1, 0]]), cap_m=1, method=method, max_tests=0
                )
        np.testing.assert_array_equal(segments, before)


if __name__ == "__main__":
    unittest.main()
