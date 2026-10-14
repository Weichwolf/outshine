"""Projection uses relative normal resolution and preserves physically small facets."""

import unittest

import numpy as np

from facade_surface import project


def mesh(points):
    return dict(vertices=np.asarray(points,dtype=float),faces=np.array([[0,1,2]],dtype=np.int32),materials=np.array([1],dtype=np.uint16))


class FacadeProjection(unittest.TestCase):
    def test_resolvable_small_horizontal_triangle_keeps_all_geometry_and_metric_uvs(self):
        source=mesh([[0.,0.,2.],[1e-9,0.,2.],[0.,1e-9,2.]])
        output,report=project(source)
        np.testing.assert_array_equal(output['vertices'][output['faces']],source['vertices'][source['faces']])
        np.testing.assert_allclose(output['uv'],source['vertices'][:,:2]/[2.4,1.2],atol=1e-25,rtol=1e-12)
        self.assertEqual(report['projected_triangles'],1)

    def test_exact_collinearity_is_rejected(self):
        with self.assertRaisesRegex(ValueError,'unresolved'):project(mesh([[0,0,0],[1,0,0],[2,0,0]]))

    def test_cross_product_rounding_does_not_establish_a_usable_normal(self):
        source=mesh([[4.888888888888888,4.,-2.3333333333333335],[5.333333333333333,4.,-2.],[4.,4.,-3.]])
        with self.assertRaisesRegex(ValueError,'unresolved'):project(source)


if __name__=='__main__':unittest.main()
