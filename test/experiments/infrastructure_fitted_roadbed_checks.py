"""Check trimmed Hermite roadbeds against complete bodies, seams and work budgets."""

import numpy as np
import shapely

from infrastructure_fitted_roadbeds import construct_fitted_roadbed,spline_roadbed
from infrastructure_source_roadbeds import roadbed_body_check,sample_roadbed_piece
from infrastructure_spline_routes import checked_spline_path,continuous_spline_joins
from infrastructure_turn_roadbeds import geometry_precision,roadbed_site_check


def verify_fitted_roadbeds(vehicle,mesh_check):
    controls=np.stack((np.linspace([0.,0.],[6.,0.],6),np.linspace([6.,0.],[12.,0.],6)))
    piece=dict(profile='G2-quintic-spline',fit=dict(controls=controls),begin=controls[0,0],end=controls[-1,-1])
    roadbed,audit=spline_roadbed(controls,vehicle,.025)
    assert roadbed is not None and audit['hull_intervals']<=255
    assert continuous_spline_joins(controls)
    positions,headings=sample_roadbed_piece(piece,np.array([0.,.5,1.]))
    assert np.allclose(positions,[[0.,0.],[6.,0.],[12.,0.]]) and np.allclose(headings,0)
    mesh_check(roadbed,vehicle,[piece],.025)
    floor=shapely.box(-3,-.925,17,.925)
    reserved=shapely.box(4,.935,8,2)
    base,reservation,support=shapely.set_precision(np.array([floor,reserved,roadbed],dtype=object),geometry_precision(floor))
    trimmed=support.difference(reservation)
    planned,addition,reason=roadbed_site_check(trimmed,base,reservation,vehicle,.025)
    assert reason is None and support.area>trimmed.area and addition.intersection(reservation).area==0
    mesh_check(planned,vehicle,[piece],.025)
    obstructed=planned.difference(shapely.box(5.99,-.02,6.01,.02))
    path,note=checked_spline_path(controls,obstructed,vehicle,.025)
    assert path is None and note['reason']=='spline_vehicle_body_outside_surface'
    for defective in (controls+np.array([[[0.,0.]],[[.1,0.]]]),
                      np.array([controls[0],controls[1,::-1]])):
        assert not continuous_spline_joins(defective)
        path,note=checked_spline_path(defective,roadbed,vehicle,.025)
        assert path is None and note['reason']=='discontinuous_spline_route'
    bent=controls.copy();bent[1,2,1]=.1
    assert not continuous_spline_joins(bent)
    mismatch=roadbed_body_check(roadbed,[dict(piece,end=np.array([13.,0.]))],vehicle,.025)
    assert not mismatch['fits_surface'] and mismatch['reason']=='spline_endpoints_differ_from_ports'
    rejected,note=spline_roadbed(controls,vehicle,.025,maximum_segments=1)
    assert rejected is None and note['reason']=='roadbed_sampling_budget_exhausted'
    body=roadbed_body_check(roadbed,[piece,piece],vehicle,.025,maximum_poses=6)
    assert not body['fits_surface'] and body['sampled_poses']<=6
    entry=dict(position=controls[0,0],heading=np.array([1.,0.]))
    exit=dict(position=controls[-1,-1],heading=np.array([1.,0.]))
    source=shapely.LineString([entry['position'],exit['position']])
    candidate,note=construct_fitted_roadbed(entry,exit,floor,shapely.Polygon(),source,vehicle,maximum_calls=1)
    assert note['residual_calls']<=1
    if candidate is not None:mesh_check(candidate['surface'],vehicle,candidate['pieces'],.025)
    return dict(fitted_roadbed_trim_checked=True,spline_seams_checked=True,
                fitted_roadbed_budgets_checked=True,interior_obstruction_rejected=True)
