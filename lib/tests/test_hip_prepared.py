import unittest
from unittest.mock import patch

import numpy as np
import shapely

from outshine_ref.buildings.modelling import Shape,hip_footprints,prepare_hip


class PreparedHip(unittest.TestCase):
    def test_meshes_reuse_prepared_plans_without_any_solver_call(self):
        polygon=shapely.Polygon([(0,0),(12,0),(12,8),(0,8)],[[(2,2),(2,6),(7,6),(7,2)]])
        source=Shape.from_polygon(polygon).r(13.,24.,-7.)
        plans=tuple(prepare_hip(p) for p in hip_footprints(source))
        for even in [False,True]:
            for overhang in [0.,-.5]:
                expected=source.roof_hip(3.,'byHeight',even=even,overhang=overhang).emit()
                with patch('outshine_ref.buildings.modelling.roof_hip.prepare_hip',side_effect=AssertionError('Unexpected solver call')):
                    actual=source.roof_hip(3.,'byHeight',even=even,overhang=overhang,prepared_plans=plans).emit()
                for key in expected:
                    if isinstance(expected[key],np.ndarray):np.testing.assert_array_equal(actual[key],expected[key])

    def test_mismatched_and_missing_prepared_domains_are_rejected(self):
        source=Shape.from_polygon(shapely.box(0,0,12,5));plan=prepare_hip(hip_footprints(source)[0])
        for entries in [(),(None,),(plan,plan)]:
            with self.assertRaises(ValueError):source.roof_hip(3.,'byHeight',prepared_plans=entries)
        different=Shape.from_polygon(shapely.box(0,0,13,5))
        with self.assertRaises(ValueError):different.roof_hip(3.,'byHeight',prepared_plans=(plan,))
