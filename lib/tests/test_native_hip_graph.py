import unittest

import numpy as np
import shapely

from outshine_ref.buildings.modelling import SkeletonBudgetExceeded,SkeletonLimits,prepare_hip


def courtyard():
    hole=[(6.3816405375158,7.138778040313788),(5.117961890258449,5.588579254200913),
          (8.2183594624842,3.061221959686211),(9.48203810974155,4.611420745799086)]
    return shapely.Polygon([(0.,0.),(16.7,0.),(16.7,12.3),(0.,12.3)],[hole])


class NativeHipGraph(unittest.TestCase):
    def test_rotated_courtyard_keeps_all_eaves_in_the_native_path(self):
        source=courtyard();before=source.wkb;plan=prepare_hip(source)
        self.assertEqual(plan.method,'native')
        self.assertIsNone(plan.fallback_reason)
        self.assertEqual({f.boundary for f in plan.facets},{(r,e) for r in range(2) for e in range(4)})
        union=shapely.union_all([f.polygon for f in plan.facets])
        self.assertLess(union.symmetric_difference(plan.footprint).area,1e-7)
        self.assertEqual(source.wkb,before)
        reference=prepare_hip(source,'skeleton')
        facets={f.boundary:f.polygon for f in reference.facets}
        for face in plan.facets:
            self.assertLess(shapely.hausdorff_distance(face.polygon,facets[face.boundary]),1e-7)
            np.testing.assert_allclose(plan.positions_m[list(face.node_indices)]@face.gradient+face.intercept_m,
                plan.distances_m[list(face.node_indices)],atol=1e-7,rtol=0.)

    def test_native_graph_node_budget_remains_an_explicit_rejection(self):
        with self.assertRaises(SkeletonBudgetExceeded):
            prepare_hip(courtyard(),limits=SkeletonLimits(nodes=4))


if __name__=='__main__':unittest.main()
