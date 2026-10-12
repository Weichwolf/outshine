"""Independent integration, rigid transforms and bounded curvature for turning recipes."""

from pathlib import Path
import sys

import numpy as np
import shapely
from scipy.integrate import quad

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from continuous_turn_recipe import advance,fit_turn
from transport_three_arc_paths import three_arc_seeds
from transport_spline_routes import continuous_spline_joins,curve_bounds,spline_sampler


def verify():
    rng=np.random.default_rng(2343)
    for _ in range(20):
        start,end=rng.uniform(-.25,.25,2);length=rng.uniform(.01,12);heading=rng.uniform(-np.pi,np.pi)
        position,angle=advance(np.zeros(2),heading,start,end,length)
        theta=lambda s:heading+start*s+(end-start)*s*s/(2*length)
        exact=np.array([quad(lambda s:np.cos(theta(s)),0,length,epsabs=1e-12)[0],
                        quad(lambda s:np.sin(theta(s)),0,length,epsabs=1e-12)[0]])
        assert np.allclose(position,exact,rtol=0,atol=1e-11)
        assert abs(angle-theta(length))<1e-12
    entry=dict(position=np.zeros(2),heading=np.array([1.,0.]))
    exit=dict(position=np.array([2.,3.]),heading=np.array([-1.,0.]))
    paths=[(kind,path) for kind,path in three_arc_seeds(entry,exit,4.5) if path.is_simple]
    kind,seed=min(paths,key=lambda p:p[1].length)
    controls,note=fit_turn(entry,exit,seed,4.5,kind[0],1.)
    assert controls is not None and note['residual_calls']<=128
    assert continuous_spline_joins(controls) and np.max(curve_bounds(controls)[2])<=.25
    xy,angles=spline_sampler(controls)(np.array([0,len(controls)-1]),np.array([0.,1.]))
    assert np.allclose(xy,[entry['position'],exit['position']],atol=1e-7,rtol=0)
    assert np.allclose(np.c_[np.cos(angles),np.sin(angles)],[entry['heading'],exit['heading']],atol=1e-8,rtol=0)
    d=5*np.diff(controls,axis=1);dd=20*np.diff(controls,n=2,axis=1)
    for i,j in ((0,0),(-1,-1)):
        assert abs(d[i,j,0]*dd[i,j,1]-d[i,j,1]*dd[i,j,0])<1e-7
    angle=.73;rotation=np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]])
    offset=np.array([1000.,-700.])
    moved=[dict(position=p['position']@rotation.T+offset,heading=p['heading']@rotation.T) for p in (entry,exit)]
    shifted=shapely.LineString(np.array(seed.coords)@rotation.T+offset)
    translated,_=fit_turn(*moved,shifted,4.5,kind[0],1.)
    assert translated is not None and translated.shape==controls.shape
    assert np.allclose(translated,controls@rotation.T+offset,atol=1e-7,rtol=0)
    _,limited=fit_turn(entry,exit,seed,4.5,kind[0],1.,maximum_calls=4)
    assert limited['residual_calls']<=4
    return dict(independent_clothoid_integration=20,endpoint_pose_closed=True,G2_joins=True,
                conservative_radius_checked=True,zero_endpoint_curvature=True,rigid_transform=True,
                actual_residual_budget=True)


if __name__=='__main__':print(verify())
