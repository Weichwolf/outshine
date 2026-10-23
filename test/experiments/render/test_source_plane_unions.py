"""Closed-interval placement, exact projected gaps and finite geometry bounds."""

import unittest

import numpy as np
import shapely
from source_plane_mesh import triangulate, union_mesh
from source_plane_union import compile_unions, weighted_offset
from source_surface_patches import SurfacePatch


def plate(domain, *, z=0, normal=(0, 0, 1)):
    return SurfacePatch(
        domain,
        np.array([0, 0, z], float),
        np.array([[1, 0, 0], [0, 1, 0]], float),
        np.array(normal, float),
        1,
    )


class PlaneUnionTest(unittest.TestCase):
    def test_shared_closed_endpoint_includes_starts_before_ends(self):
        self.assertEqual(weighted_offset([0, 1], [1, 2], [2, 3]), 1)

    def test_interval_choice_matches_independent_endpoint_enumeration(self):
        rng = np.random.default_rng(123)
        for _ in range(20):
            low = rng.uniform(-20, 20, 100)
            high = low + rng.uniform(0, 6, 100)
            weights = rng.uniform(0.01, 100, 100)
            chosen = weighted_offset(low, high, weights)
            score = weights[(low <= chosen) & (chosen <= high)].sum()
            brute = max(
                weights[(low <= p) & (p <= high)].sum() for p in np.r_[low, high]
            )
            self.assertAlmostEqual(score, brute, places=9)

    def test_overlapping_source_plates_become_one_contour(self):
        a = plate(shapely.box(0, 0, 2, 2))
        b = plate(shapely.box(1, 0, 3, 2), z=1)
        groups, membership, work = compile_unions([a, b], epsilon=0.5)
        self.assertEqual(len(groups), 1)
        self.assertEqual(groups[0].domain.area, 6)
        self.assertEqual(work["maximum_vertex_projection_m"], 0.5)
        np.testing.assert_array_equal(membership, [0, 0])

    def test_two_centimetre_projected_gap_is_not_filled_by_union(self):
        a = plate(shapely.box(0, 0, 1, 2))
        b = plate(shapely.box(1.02, 0, 2.02, 2), z=1)
        groups, _, _ = compile_unions([a, b])
        self.assertEqual(len(groups), 1)
        self.assertEqual(len(shapely.get_parts(groups[0].domain)), 2)
        gap = np.array([1.01, 1, 0]) @ groups[0].axes.T
        inside = np.array([0.5, 1, 0]) @ groups[0].axes.T
        self.assertFalse(shapely.covers(groups[0].domain, shapely.Point(gap)))
        self.assertTrue(shapely.covers(groups[0].domain, shapely.Point(inside)))

    def test_projected_courtyard_hole_survives_constrained_triangulation(self):
        domain = shapely.Polygon(
            [(0, 0), (4, 0), (4, 4), (0, 4)], [[(1, 1), (3, 1), (3, 3), (1, 3)]]
        )
        triangles = triangulate(domain)
        rebuilt = shapely.union_all(shapely.polygons(triangles))
        self.assertAlmostEqual(rebuilt.area, 12)
        self.assertFalse(shapely.covers(rebuilt, shapely.Point(2, 2)))
        self.assertAlmostEqual(shapely.symmetric_difference(rebuilt, domain).area, 0)

    def test_opposing_orientations_are_separate_without_mean_normal(self):
        a = plate(shapely.box(0, 0, 2, 2))
        b = plate(shapely.box(0, 0, 2, 2), normal=(0, 0, -1))
        groups, membership, _ = compile_unions([a, b])
        self.assertEqual(len(groups), 2)
        self.assertNotEqual(*membership)
        self.assertTrue(any(g.normal[2] == 1 for g in groups))
        self.assertTrue(any(g.normal[2] == -1 for g in groups))

    def test_float32_geometry_and_winding_are_reported(self):
        groups, _, _ = compile_unions([plate(shapely.box(0, 0, 2, 2))])
        arrays, report = union_mesh(groups)
        triangle = arrays["positions"][arrays["indices"]].astype(float)
        normal = np.cross(
            triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0]
        )
        self.assertTrue((normal[:, 2] > 0).all())
        self.assertEqual(arrays["positions"].dtype, np.float32)
        self.assertEqual(report["float32_degenerate_triangles"], 0)

    def test_invalid_projection_parameters_fail(self):
        for epsilon in (0, -1, np.inf):
            with self.subTest(epsilon=epsilon), self.assertRaises(ValueError):
                compile_unions([plate(shapely.box(0, 0, 1, 1))], epsilon=epsilon)

    def test_whole_tilted_patch_survives_when_family_slab_is_too_thin(self):
        angle = np.radians(0.4)
        axes = np.array([[np.cos(angle), 0, np.sin(angle)], [0, 1, 0]])
        tilted = SurfacePatch(
            shapely.box(0, 0, 1000, 1000), np.zeros(3), axes, np.cross(*axes), 2
        )
        groups, membership, work = compile_unions(
            [plate(shapely.box(0, 0, 2000, 2000)), tilted], epsilon=0.001
        )
        self.assertEqual(work["fallback_patches"], 1)
        kept = groups[membership[1]]
        self.assertTrue(kept.fallback)
        self.assertAlmostEqual(kept.domain.area, 1e6, places=6)
        np.testing.assert_allclose(kept.normal, tilted.normal)
