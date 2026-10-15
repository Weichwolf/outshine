import unittest

import numpy as np
import shapely

from outshine_ref.buildings.facades import FacadeRepeat,rectangle_coverage


def explicit(repeat,centres,footprints):
    rectangles=[];pw,ph=repeat.cell_size_m
    for row in range(repeat.rows):
        for column in range(repeat.columns):
            x=(column+.5)*pw;y=(row+.5)*ph;w=pw*repeat.opening_width;h=ph*repeat.opening_height
            if w*h:rectangles.append(shapely.box(x-w/2,y-h/2,x+w/2,y+h/2))
    region=shapely.union_all(rectangles);values=[]
    for centre,foot in zip(centres,footprints,strict=True):
        low=centre-foot/2;high=centre+foot/2
        values.append(region.intersection(shapely.box(*low,*high)).area/np.prod(foot))
    return np.asarray(values)


class Facades(unittest.TestCase):
    def test_finite_boxes_match_independent_explicit_rectangles(self):
        rng=np.random.default_rng(41);centres=rng.uniform([-2,-2],[18,13],(200,2));footprints=rng.uniform(.001,12,(200,2))
        for fractions in ((0.,.7),(.43,.61),(1.,1.),(.99,.01)):
            repeat=FacadeRepeat(16.,10.,5,3,*fractions)
            np.testing.assert_allclose(repeat.coverage(centres,footprints),explicit(repeat,centres,footprints),atol=1e-12,rtol=1e-10)

    def test_complete_domain_preserves_analytic_opening_area(self):
        repeat=FacadeRepeat(22.,15.,7,5,.43,.61)
        self.assertEqual(repeat.opening_count,35)
        self.assertAlmostEqual(float(repeat.coverage([11.,7.5],[22.,15.])),.43*.61,places=14)
        self.assertAlmostEqual(float(repeat.coverage([11.,7.5],[44.,30.])),.43*.61/4,places=14)

    def test_periodic_boxes_and_translation_leave_area_mean_unchanged(self):
        centres=np.array([[.5,.5],[-.5,-.5],[.2,.8],[.99,.01]])
        for footprint in ((1.,1.),(7.,3.),(32.,32.)):
            actual=rectangle_coverage(centres,footprint,[.43,.61])
            np.testing.assert_allclose(actual,.43*.61,atol=1e-14,rtol=0)
            for shift in ((0.,0.),(217.,-511.),(2**18,-2**19)):
                np.testing.assert_allclose(rectangle_coverage(centres+shift,footprint,[.43,.61]),actual,atol=2e-11,rtol=0)

    def test_subpixel_window_and_boundary_cases(self):
        fractions=[.4,.6]
        np.testing.assert_array_equal(rectangle_coverage([[.5,.5],[.01,.01]],[[1e-8,1e-8]],[.4,.6]),[1.,0.])
        self.assertAlmostEqual(float(rectangle_coverage([.3,.5],[.2,.1],fractions)),.5,places=12)
        self.assertEqual(float(rectangle_coverage([-.5,.5],[.1,.1],fractions,[3,2])),0.)

    def test_metric_replay_scaling_and_broadcasting(self):
        a=FacadeRepeat(16.,10.,5,3,.43,.61);b=FacadeRepeat(160.,100.,5,3,.43,.61)
        centres=[[4.,3.],[7.,9.]];foot=[[.01,.01],[5.,4.]]
        np.testing.assert_array_equal(a.coverage(centres,foot),b.coverage(np.asarray(centres)*10,np.asarray(foot)*10))
        self.assertEqual(a.coverage(centres,[.5,.5]).shape,(2,))

    def test_invalid_layout_and_filter_inputs_reject(self):
        for fields in [(0.,10.,2,2,.4,.5),(10.,float('nan'),2,2,.4,.5),(10.,10.,True,2,.4,.5),
                       (10.,10.,2,0,.4,.5),(10.,10.,2,2,-.1,.5)]:
            with self.assertRaises(ValueError):FacadeRepeat(*fields)
        for centres,foot,fractions,counts in [(0.,[1,1],[.4,.5],None),([0,0],[0,1],[.4,.5],None),
            ([0,0],[1,1],[float('nan'),.5],None),([0,0],[1,1],[.4,.5],[1.5,2]),([0,0],[1,1],[.4,.5],[True,True])]:
            with self.assertRaises(ValueError):rectangle_coverage(centres,foot,fractions,counts)


if __name__=='__main__':unittest.main()
