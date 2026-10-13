"""Independent integration, rigid transforms and bounded curvature for turning recipes."""

from pathlib import Path
import sys

import numpy as np
import shapely
from scipy.integrate import quad

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from continuous_turn_recipe import advance,fit_turn,controls_for,endpoint
from transport_fitted_roadbeds import spline_roadbed
from transport_three_arc_paths import three_arc_seeds
from transport_arc_line_paths import arc_line_arc_seeds
from transport_spline_routes import continuous_spline_joins,curve_bounds,spline_sampler
from coupled_turning_court_probe import approach_segment


def verify_approaches():
    a=dict(position=np.zeros(2),heading=np.array([1.,0.]))
    b=dict(position=np.array([6.,1.]),heading=np.array([np.cos(.2),np.sin(.2)]))
    controls=approach_segment(a,b);d=5*np.diff(controls,axis=0);dd=20*np.diff(controls,n=2,axis=0)
    assert np.allclose(controls[[0,-1]],[a['position'],b['position']])
    assert np.allclose(d[0]/np.linalg.norm(d[0]),a['heading'])
    assert np.allclose(d[-1]/np.linalg.norm(d[-1]),b['heading'])
    assert np.allclose(dd[[0,-1]],0,atol=1e-12) and np.max(curve_bounds(controls[None])[2])<.25
    straight=dict(position=np.array([6.,0.]),heading=np.array([1.,0.]))
    assert np.allclose(approach_segment(a,straight),np.linspace(a['position'],straight['position'],6))
    rotation=np.array([[0.,-1.],[1.,0.]]);offset=np.array([1000.,-700.])
    shifted=[dict(position=p['position']@rotation+offset,heading=p['heading']@rotation) for p in (a,b)]
    assert np.allclose(approach_segment(*shifted),controls@rotation+offset,atol=1e-12,rtol=0)
    return dict(bent_approach_endpoints=True,bent_approach_tangents=True,zero_approach_curvature=True,
                bounded_approach_radius=True,straight_approach_unchanged=True,approach_rigid_transform=True)


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


def verify_arc_line_arcs():
    entry=dict(position=np.zeros(2),heading=np.array([1.,0.]))
    exit=dict(position=np.array([25.,15.]),heading=np.array([0.,1.]))
    rotation=np.array([[0.,-1.],[1.,0.]]);offset=np.array([1000.,-700.])
    moved=[dict(position=p['position']@rotation+offset,heading=p['heading']@rotation) for p in (entry,exit)]
    tested=0
    for kind,seed in arc_line_arc_seeds(entry,exit,4.5):
        controls,note=fit_turn(entry,exit,seed,4.5,kind,1.)
        assert controls is not None and note['residual_calls']<=128,note
        assert continuous_spline_joins(controls) and np.max(curve_bounds(controls)[2])<=.25
        xy,angles=spline_sampler(controls)(np.array([0,len(controls)-1]),np.array([0.,1.]))
        assert np.allclose(xy,[entry['position'],exit['position']],atol=1e-7,rtol=0)
        assert np.allclose(np.c_[np.cos(angles),np.sin(angles)],[entry['heading'],exit['heading']],atol=1e-8,rtol=0)
        shifted=shapely.LineString(np.array(seed.coords)@rotation+offset)
        transformed,_=fit_turn(*moved,shifted,4.5,kind,1.)
        assert transformed is not None and transformed.shape==controls.shape
        assert np.allclose(transformed,controls@rotation+offset,atol=1e-7,rtol=0)
        _,limited=fit_turn(entry,exit,seed,4.5,kind,1.,maximum_calls=4)
        assert limited['residual_calls']<=4
        tested+=1
    return dict(csc_turn_directions=tested,csc_fixed_endpoint_poses=True,csc_g2_radius=True,
                csc_rigid_transform=True,csc_residual_budget=True)


def verify_long_straight():
    entry=dict(position=np.zeros(2),heading=np.array([1.,0.]))
    lengths=np.array([2.,1000.,3.]);radius=4.5;sign=(1,-1)
    controls=controls_for(lengths,radius,sign,1.,entry)
    assert len(controls)<32 and continuous_spline_joins(controls)
    xy,angle=spline_sampler(controls)(np.array([len(controls)-1]),np.array([1.]))
    exact=endpoint(lengths,radius,sign,1.)
    assert np.allclose(xy[0],exact[:2],atol=1e-8,rtol=0)
    assert abs(np.arctan2(np.sin(angle[0]-exact[2]),np.cos(angle[0]-exact[2])))<1e-10
    vehicle=dict(widthM=1.8,frontFromRearAxleM=3.3,rearOverhangM=1.1,minimumRearAxleRadiusM=4.)
    road,audit=spline_roadbed(controls,vehicle,.025)
    assert road is not None,audit
    assert audit['hull_intervals']<255 and np.max(curve_bounds(controls)[2])<=.25
    return dict(kilometre_straight_has_bounded_controls=True,long_roadbed_within_sampling_budget=True,
                independent_long_endpoint=True)


if __name__=='__main__':print(verify()|verify_approaches()|verify_arc_line_arcs()|verify_long_straight())
