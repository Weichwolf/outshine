"""Check unequal lane offsets, fixed terminal rays and unchanged interior source bends."""

import json
import numpy as np
import shapely

from terminal_lane_alignment_probe import terminal_lane_guide
from rounded_lane_loop_probe import rounded_polyline
from transport_spline_routes import continuous_spline_joins,curve_bounds


def verify():
    source=shapely.LineString([[0,0],[0,10],[-40,10],[-40,20]])
    entry=dict(position=np.array([0.,0.]),heading=np.array([0.,1.]))
    exit=dict(position=np.array([-38.625,20.]),heading=np.array([0.,1.]))
    points=terminal_lane_guide(source,entry,exit)
    assert np.allclose(points,[[0,0],[0,10],[-38.625,10],[-38.625,20]])
    controls=rounded_polyline(points,4.2,.5,entry,exit)
    assert continuous_spline_joins(controls) and curve_bounds(controls)[2].max()<.25
    extended=shapely.LineString([[0,0],[0,10],[-20,10],[-20,15],[-40,15],[-40,20]])
    guide=terminal_lane_guide(extended,entry,exit)
    assert np.array_equal(guide[2:-2],shapely.get_coordinates(extended)[2:-2])
    angle=.7;matrix=np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]]);offset=np.array([1000.,-700.])
    transform=lambda p:p@matrix.T+offset
    poses=[dict(position=transform(p['position']),heading=p['heading']@matrix.T) for p in (entry,exit)]
    moved=terminal_lane_guide(shapely.transform(source,transform),*poses)
    assert np.allclose(moved,transform(points),atol=1e-10,rtol=0)
    try:terminal_lane_guide(shapely.LineString([[0,0],[0,10],[10,10]]),entry,exit)
    except AssertionError as error:assert str(error)=='terminal lane recipe requires separate source bends'
    else:raise AssertionError('a single bend was treated as two independent terminal bends')
    return dict(unequal_lane_offsets=True,fixed_rays=True,interior_bends_retained=True,
                rigid_transform=True,g2_radius_checked=True,single_bend_rejected=True)


if __name__=='__main__':print(json.dumps(verify()))
