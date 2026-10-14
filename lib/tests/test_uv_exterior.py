import unittest

import numpy as np
import shapely

from outshine_ref.buildings.modelling import extrapolate_chart,partition_exterior_charts


class ExteriorUV(unittest.TestCase):
    def setUp(self):
        self.corners=np.array([[[0.,0.],[6.,0.],[6.,4.]],[[6.,0.],[12.,0.],[12.,4.]]])
        self.edge=(np.array([0.,0.]),np.array([12.,0.]))

    def test_corner_continuation_and_source_chart_seam(self):
        charts=partition_exterior_charts(self.corners,*self.edge,shapely.box(-1,-1,13,0))
        self.assertEqual([c.triangle for c in charts],[0,1])
        self.assertAlmostEqual(charts[0].polygon.area,7.)
        self.assertAlmostEqual(charts[1].polygon.area,7.)
        properties=self.corners.copy();properties[1]+=100.
        np.testing.assert_allclose(extrapolate_chart(self.corners,properties,0,[[-1.,-.5]]),[[-1.,-.5]])
        np.testing.assert_allclose(extrapolate_chart(self.corners,properties,1,[[13.,-.5]]),[[113.,99.5]])

    def test_nested_collection_retains_each_polygon_and_ignores_zero_area(self):
        domain=shapely.GeometryCollection([shapely.MultiPolygon([shapely.box(-1,-1,2,0),shapely.box(8,-1,13,0)]),
                                           shapely.LineString([(3.,-.5),(4.,-.5)])])
        charts=partition_exterior_charts(self.corners,*self.edge,domain)
        self.assertEqual(len(charts),2)
        self.assertAlmostEqual(sum(c.polygon.area for c in charts),8.)

    def test_invalid_chart_inputs_and_capacity_are_rejected(self):
        properties=self.corners.copy()
        for triangle in (True,-1,2,.5):
            with self.assertRaises(ValueError):extrapolate_chart(self.corners,properties,triangle,[[0.,0.]])
        with self.assertRaises(ValueError):
            extrapolate_chart(np.zeros((1,3,2)),np.zeros((1,3,2)),0,[[0.,0.]])
        with self.assertRaises(ValueError):
            partition_exterior_charts(self.corners,*self.edge,shapely.box(-1,-1,13,0),max_regions=1)
        with self.assertRaises(ValueError):
            extrapolate_chart(self.corners,properties,0,[[float('nan'),0.]])


if __name__=='__main__':unittest.main()
