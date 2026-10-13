from fractions import Fraction
import math
import unittest

import numpy as np

from outshine_ref.buildings.modelling.scope import scalar


class ModellingNumbers(unittest.TestCase):
    def test_common_and_abstract_real_numbers_keep_their_value(self):
        for value in (2,2.,np.int64(2),np.uint64(2),np.float32(2),np.float64(2),np.longdouble(2),Fraction(4,2)):
            self.assertEqual(scalar(value),2.);self.assertIs(type(scalar(value)),float)
        self.assertEqual(math.copysign(1.,scalar(-0.)),-1.)

    def test_boolean_text_complex_and_nonfinite_numbers_remain_invalid(self):
        for value in (True,False,np.bool_(True),'2',complex(2),None,np.array(2),float('nan'),float('inf'),np.float64('nan')):
            with self.assertRaises(ValueError):scalar(value)


if __name__=='__main__':unittest.main()
