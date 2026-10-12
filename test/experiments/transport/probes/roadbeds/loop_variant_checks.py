"""Verify exact curve subdivision and source lane geometry under rigid transforms."""

import json
import numpy as np
import shapely

from reuse_lane_loop_probe import tighten_curvature_bound
from sliding_corner_loop_probe import lane_vertices
from transport_hermite_route_fitting import hermite_controls
from transport_spline_routes import curve_bounds,evaluate_controls,continuous_spline_joins


def verify():
    angle=.4;radius=4.1;length=radius*angle
    points=radius*np.array([[1.,0.],[np.cos(angle),np.sin(angle)]])
    headings=np.array([[0.,1.],[-np.sin(angle),np.cos(angle)]])
    controls=hermite_controls(np.c_[points,headings*length,-points/radius**2*length**2])
    refined=tighten_curvature_bound(controls,4.)
    assert curve_bounds(controls)[2].max()>.25 and curve_bounds(refined)[2].max()<.25
    assert len(refined)==2 and continuous_spline_joins(refined)
    for i,piece in enumerate(refined):
        t=np.linspace(0,1,129)
        a=evaluate_controls(np.broadcast_to(controls[0],(len(t),6,2)),(i+t)/2)
        b=evaluate_controls(np.broadcast_to(piece,(len(t),6,2)),t)
        assert np.allclose(a,b,atol=1e-12,rtol=0),'subdivision altered the curve'
    source=shapely.LineString([[0,0],[20,0],[20,20],[0,20],[0,3]])
    entry=dict(position=np.array([0.,-1.]),heading=np.array([1.,0.]))
    exit=dict(position=np.array([-1.,3.]),heading=np.array([0.,-1.]))
    lane=lane_vertices(source,entry,exit)
    assert np.allclose(lane[[0,-1]],[entry['position'],exit['position']])
    matrix=np.array([[0.,-1.],[1.,0.]]);offset=np.array([1000.,-700.])
    transform=lambda p:p@matrix.T+offset
    poses=[dict(position=transform(p['position']),heading=p['heading']@matrix.T) for p in (entry,exit)]
    moved=lane_vertices(shapely.transform(source,transform),*poses)
    assert np.allclose(moved,transform(lane),atol=1e-10,rtol=0)
    return dict(exact_subdivision=True,conservative_radius=True,g2_preserved=True,source_lanes=True,rigid_transform=True)


if __name__=='__main__':print(json.dumps(verify()))
