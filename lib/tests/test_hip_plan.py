import subprocess
import sys
import unittest

import numpy as np
import shapely
from shapely import affinity

from outshine_ref.buildings.modelling import SkeletonBudgetExceeded, SkeletonLimits, prepare_hip


def volume(plan, value, mode):
    return sum(f.polygon.area*(np.asarray(f.gradient)@np.asarray(f.polygon.centroid.coords[0])+f.intercept_m)
               for f in plan.facets)*plan.factor(value, mode)


class HipPlanning(unittest.TestCase):
    def test_rectangle_and_square_match_analytic_height_and_volume(self):
        for length, width in ((12., 5.), (5., 5.)):
            for angle in (0., 31., 90.):
                polygon = affinity.rotate(shapely.box(0, 0, length, width), angle)
                for method in ('rectangle', 'skeleton'):
                    plan = prepare_hip(polygon, method)
                    self.assertEqual(len(plan.facets), 4)
                    self.assertAlmostEqual(plan.distances_m.max(), width/2.)
                    for value, mode in ((3., 'byHeight'), (25., 'byAngle')):
                        rise = value if mode == 'byHeight' else np.tan(np.deg2rad(value))*width/2.
                        self.assertAlmostEqual(volume(plan, value, mode), rise*(length*width/2-width**2/6), places=8)

    def test_concavity_holes_and_mirror_keep_the_exact_domain(self):
        polygon = shapely.Polygon([(0, 0), (10, 0), (10, 8), (0, 8)], [[(2, 2), (2, 6), (7, 6), (7, 2)]])
        elbow = shapely.Polygon([(0, 0), (10, 0), (10, 3), (5, 3), (5, 8), (0, 8)])
        for source in (polygon, elbow, affinity.scale(polygon, -1, 1), affinity.translate(polygon, 1e8, -1e8)):
            before = source.wkb
            plan = prepare_hip(source)
            union = shapely.union_all([facet.polygon for facet in plan.facets])
            self.assertLess(union.symmetric_difference(plan.footprint).area, 1e-7)
            self.assertEqual(len(plan.facets), sum(len(r.coords)-1 for r in (source.exterior, *source.interiors)))
            self.assertEqual(before, source.wkb)
            self.assertAlmostEqual(plan.lifted_vertices(3., 'byHeight')[:, 2].max(), 3.)
            for facet in plan.facets:
                np.testing.assert_allclose(plan.positions_m[list(facet.node_indices)]@facet.gradient+facet.intercept_m,
                                           plan.distances_m[list(facet.node_indices)], atol=1e-7, rtol=0)

    def test_arrays_are_immutable_and_lifting_reuses_the_plan(self):
        plan = prepare_hip(shapely.box(0, 0, 12, 5))
        for values in (plan.positions_m, plan.distances_m):
            with self.assertRaises(ValueError):
                values.flags.writeable = True
        first, second = plan.lifted_vertices(3., 'byHeight'), plan.lifted_vertices(6., 'byHeight')
        np.testing.assert_array_equal(first[:, :2], second[:, :2])
        np.testing.assert_allclose(second[:, 2], first[:, 2]*2.)
        first[:] = 0
        self.assertGreater(plan.positions_m.max(), 0)

    def test_collinear_source_edges_share_a_consistent_roof_plane(self):
        polygon = shapely.Polygon([(0, 0), (12, 0), (12, 5), (8, 5), (4, 5), (0, 5)])
        plan = prepare_hip(polygon)
        self.assertEqual(len(plan.facets), 6)
        self.assertAlmostEqual(volume(plan, 3., 'byHeight'), 77.5)
        np.testing.assert_allclose([np.linalg.norm(f.gradient) for f in plan.facets], 1., atol=1e-12)

    def test_triangle_has_one_peak_and_the_analytic_pyramid_volume(self):
        polygon = shapely.Polygon([(0, 0), (12, 0), (8, 7)])
        plan = prepare_hip(polygon)
        self.assertEqual(len(plan.facets), 3)
        self.assertEqual(np.count_nonzero(plan.distances_m), 1)
        self.assertAlmostEqual(volume(plan, 3., 'byHeight'), polygon.area)

    def test_budget_errors_restore_the_backend_tracer(self):
        from py_straight_skeleton import algorithm
        original = algorithm.GLOBAL_ALGORITHM_TRACER
        polygon = shapely.box(0, 0, 12, 5)
        for limits in (SkeletonLimits(python_events=0), SkeletonLimits(python_events=1), SkeletonLimits(nodes=1), SkeletonLimits(seconds=1e-12)):
            with self.assertRaises(SkeletonBudgetExceeded):
                prepare_hip(polygon, 'skeleton', limits)
            self.assertIs(algorithm.GLOBAL_ALGORITHM_TRACER, original)
        self.assertEqual(len(prepare_hip(polygon, 'skeleton').facets), 4)

    def test_invalid_domains_values_and_methods_are_explicit(self):
        for source in (shapely.Polygon(), shapely.Polygon([(0, 0), (1, 1), (0, 1), (1, 0)]), shapely.Point(0, 0)):
            with self.assertRaises(ValueError):
                prepare_hip(source)
        with self.assertRaises(SkeletonBudgetExceeded):
            prepare_hip(shapely.box(0, 0, 12, 5), limits=SkeletonLimits(vertices=3))
        with self.assertRaises(ValueError):
            prepare_hip(shapely.box(0, 0, 12, 5), 'unknown')
        plan = prepare_hip(shapely.box(0, 0, 12, 5))
        for value, mode in ((0., 'byHeight'), (-1., 'byHeight'), (90., 'byAngle'), (float('nan'), 'byAngle'), (True, 'byAngle'), (1., 'unknown')):
            with self.assertRaises(ValueError):
                plan.lifted_vertices(value, mode)

    def test_basic_import_and_rectangle_do_not_load_the_optional_solver(self):
        command = "import sys,shapely; from outshine_ref.buildings.modelling import prepare_hip; prepare_hip(shapely.box(0,0,12,5)); assert not any(k.startswith('py_straight_skeleton') for k in sys.modules)"
        subprocess.run([sys.executable, '-I', '-c', command], check=True, capture_output=True)


if __name__ == '__main__':
    unittest.main()
