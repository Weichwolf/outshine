"""Independent rational intersection and local output-contraction counterexamples."""

from fractions import Fraction as F
from types import SimpleNamespace
import unittest

import numpy as np

from boolean_exact_regions import clip,partition,area,clean
from boolean_exact_rounding import contract
from boolean_output_repair import cancel,zero_area
from boolean_source_coordinates import projected_weights,rational_weights


class ExactControls(unittest.TestCase):
    def test_nonbinary_intersection(self):
        triangle=((F(0),F(0)),(F(2),F(1)),(F(0),F(1)))
        result=clip(triangle,(F(1),F(0)),(F(0),F(1)),1)
        self.assertIn((F(2,3),F(1,3)),result)
        self.assertEqual(abs(area(result)),F(1,3))

    def test_containment_and_contact(self):
        square=((F(0),F(0)),(F(2),F(0)),(F(2),F(2)),(F(0),F(2)))
        outer=((F(-1),F(-1)),(F(9),F(-1)),(F(-1),F(9)))
        self.assertEqual(partition(square,outer),( [],square ))
        touching=((F(2),F(0)),(F(3),F(0)),(F(2),F(2)))
        outside,inside=partition(square,touching)
        self.assertFalse(inside)
        self.assertEqual(sum(abs(area(p)) for p in outside),F(4))

    def test_rational_capacity(self):
        huge=F(1,2**4097)
        with self.assertRaisesRegex(ValueError,'bit capacity'):
            clean(((F(0),F(0)),(huge,F(0)),(F(0),F(1))))

    def test_contact_fans_stay_separate(self):
        coordinates={0:np.array([0.,0.,0.]),1:np.array([1.,0.,0.]),2:np.array([0.,1.,0.]),
                     3:np.array([0.,0.,0.]),4:np.array([-1.,0.,0.]),5:np.array([0.,-1.,0.])}
        roots=np.array([[0,1,2],[3,4,5]])
        actual,_,_=contract(roots,('a','b'),[0,1],SimpleNamespace(coordinates=coordinates))
        np.testing.assert_array_equal(actual,roots)

    def test_equality_only_and_no_mutation(self):
        coordinates={0:np.array([0.,0.,0.]),1:np.array([0.,0.,0.]),2:np.array([1.,0.,0.]),
                     3:np.array([0.,1.,0.]),4:np.array([1e-30,0.,0.])}
        roots=np.array([[0,1,2],[1,2,3],[0,4,3]]);before=roots.copy()
        actual,supports,native=contract(roots,('a','b','c'),[0,1,2],SimpleNamespace(coordinates=coordinates))
        np.testing.assert_array_equal(actual,[[0,2,3],[0,4,3]])
        self.assertEqual(supports,('b','c'));np.testing.assert_array_equal(native,[1,2])
        np.testing.assert_array_equal(roots,before)

    def test_zero_area_opposites_cancel(self):
        nodes=SimpleNamespace(coordinates={0:np.array([0.,0.,0.]),1:np.array([1.,0.,0.]),2:np.array([2.,0.,0.])})
        roots=np.array([[0,1,2],[2,1,0]]);before=roots.copy()
        actual,supports,native=cancel(roots,('a','a'),[7,7],nodes)
        self.assertEqual(actual.shape,(0,3));self.assertFalse(supports);self.assertEqual(len(native),0)
        np.testing.assert_array_equal(roots,before)

    def test_source_boundaries_do_not_cancel(self):
        nodes=SimpleNamespace(coordinates={0:np.array([0.,0.,0.]),1:np.array([1.,0.,0.]),2:np.array([2.,0.,0.])})
        roots=np.array([[0,1,2],[0,2,1]])
        for supports,native in [(('a','b'),[7,7]),(('a','a'),[7,8])]:
            actual,_,_=cancel(roots,supports,native,nodes)
            np.testing.assert_array_equal(actual,roots)

    def test_same_direction_and_single_facet_stay(self):
        nodes=SimpleNamespace(coordinates={0:np.array([0.,0.,0.]),1:np.array([1.,0.,0.]),2:np.array([2.,0.,0.])})
        for roots in (np.array([[0,1,2]]),np.array([[0,1,2],[1,2,0]])):
            actual,_,_=cancel(roots,('a',)*len(roots),[7]*len(roots),nodes)
            np.testing.assert_array_equal(actual,roots)

    def test_real_area_is_not_cancelled(self):
        nodes=SimpleNamespace(coordinates={0:np.array([0.,0.,0.]),1:np.array([1.,0.,0.]),2:np.array([0.,1e-30,0.])})
        roots=np.array([[0,1,2],[0,2,1]])
        actual,_,_=cancel(roots,('a','a'),[7,7],nodes)
        np.testing.assert_array_equal(actual,roots)

    def test_underflow_does_not_prove_zero(self):
        triangle=np.array([[0.,0.,0.],[1e-200,0.,0.],[0.,1e-200,0.]])
        with np.errstate(under='ignore'):
            self.assertFalse(zero_area(triangle))

    def test_skinny_oblique_barycentric_coordinates(self):
        corners=np.array([[0.,0.,0.],[8.,6.,0.],[8.000001,6.,0.]])
        points=np.array([[4.00000025,3.,0.],[8.0000005,6.,0.]])
        exact=rational_weights(corners,points)
        np.testing.assert_allclose(exact,[[.5,.25,.25],[0.,.5,.5]],atol=2e-9,rtol=0.)
        actual=projected_weights(corners,points)
        np.testing.assert_allclose(actual,exact,atol=1e-12,rtol=0.)
        np.testing.assert_allclose(exact@corners,points,atol=2e-15,rtol=0.)


if __name__=='__main__':unittest.main()
