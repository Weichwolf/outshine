"""Independent rational intersection and local output-contraction counterexamples."""

from fractions import Fraction as F
from types import SimpleNamespace
import unittest

import numpy as np

from boolean_exact_regions import clip,partition,area,clean
from boolean_exact_rounding import contract


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


if __name__=='__main__':unittest.main()
