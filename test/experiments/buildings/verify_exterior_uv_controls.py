"""Analytic split-chart, corner continuation, orientation and capacity controls."""

import numpy as np
import shapely

from exterior_uv_probe import partition,values


def verify():
    corners=np.array([[[0.,0.],[6.,0.],[6.,4.]],[[6.,0.],[12.,0.],[12.,4.]]])
    coefficients=np.arange(60,dtype=float).reshape(3,20)/13.
    properties=np.einsum('tvi,ij->tvj',np.concatenate((corners,np.ones((2,3,1))),axis=2),coefficients)
    properties[1]+=7.
    exterior=shapely.box(-1.,-1.,13.,0.)
    for angle in (0.,.37,1.57):
        matrix=np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]])
        for translation in (np.zeros(2),np.array([1e4,-2e4])):
            shifted=corners@matrix.T+translation
            domain=shapely.Polygon(np.asarray(exterior.exterior.coords)@matrix.T+translation)
            a,b=np.array([[0.,0.],[12.,0.]])@matrix.T+translation
            charts=partition(shifted,a,b,domain)
            assert len(charts)==2
            assert abs(shapely.union_all([c.polygon for c in charts]).area-exterior.area)<1e-7
            for x,triangle in ((-1.,0),(5.99,0),(6.01,1),(13.,1)):
                sample=np.array([[x,-.5]])@matrix.T+translation
                actual=values(shifted,properties,triangle,sample)
                expected=np.array([[x,-.5,1.]])@coefficients+7.*triangle
                np.testing.assert_allclose(actual,expected,atol=1e-7,rtol=0.)
    left=partition(corners,np.array([12.,0.]),np.array([0.,0.]),exterior)
    assert left[0].triangle==1 and left[1].triangle==0
    for parameter in (False,0,1):
        try:partition(corners,np.array([0.,0.]),np.array([12.,0.]),exterior,max_regions=parameter)
        except ValueError:pass
        else:raise AssertionError('Exterior chart capacity ignored')
    try:partition(corners,np.array([0.,0.]),np.array([11.,1.]),exterior)
    except ValueError:pass
    else:raise AssertionError('Non-eave accepted')
    assert not partition(corners,np.array([0.,0.]),np.array([12.,0.]),shapely.Polygon())
    corrupted=properties.copy();corrupted[1,:,7]+=.01
    good=values(corners,properties,1,[[7.,-.5]])
    bad=values(corners,corrupted,1,[[7.,-.5]])
    assert abs(good[0,7]-bad[0,7])>1e-7
    print('6 rotated/translated split-chart cases, reverse eave, capacity, empty and UV corruption passed')


if __name__=='__main__':verify()
