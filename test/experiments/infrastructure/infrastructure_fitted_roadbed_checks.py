"""Check trimmed Hermite roadbeds against complete bodies, seams and work budgets."""

import numpy as np
import shapely

from infrastructure_fitted_roadbeds import construct_fitted_roadbed,spline_roadbed
from infrastructure_source_roadbeds import roadbed_body_check,published_roadbed_body_check,sample_roadbed_piece
from infrastructure_spline_routes import checked_spline_path,continuous_spline_joins
from infrastructure_turn_roadbeds import geometry_precision,roadbed_site_check,coordinate_roundoff,preserves_coverage


def verify_floor_coverage():
    for offset in (0.,1000.,10000.):
        floor=shapely.box(offset,offset,offset+10,offset+10)
        tolerance=coordinate_roundoff(floor)
        assert preserves_coverage(floor,floor.union(shapely.box(offset+9,offset,offset+12,offset+10)))
        rounded=shapely.box(offset+tolerance*.25,offset,offset+10,offset+10)
        assert preserves_coverage(floor,rounded)
        for width in (tolerance*4,.01):
            narrowed=shapely.box(offset+width,offset,offset+10,offset+10)
            assert not preserves_coverage(floor,narrowed)
        side=np.sqrt(tolerance)*.25
        damaged=floor.difference(shapely.box(offset+5,offset+5,offset+5+side,offset+5+side))
        assert floor.difference(damaged).area<tolerance*floor.length
        assert not preserves_coverage(floor,damaged)
    return dict(floor_coverage_roundoff_checked=True,small_area_wide_floor_loss_rejected=True)


def verify_mesh_roundoff(vehicle):
    half=vehicle['widthM']*.5
    for offset in (0.,1000.,10000.):
        controls=np.linspace([offset,offset],[offset+10,offset],6)[None,:]
        piece=dict(profile='G2-quintic-spline',fit=dict(controls=controls),begin=controls[0,0],end=controls[0,-1])
        surface=shapely.box(offset-3,offset-half,offset+15,offset+half)
        tolerance=coordinate_roundoff(surface)
        rounded=shapely.box(offset-3,offset-half+tolerance*.25,offset+15,offset+half)
        assert not roadbed_body_check(rounded,[piece],vehicle,.025)['fits_surface']
        result=published_roadbed_body_check(rounded,[piece],vehicle,.025)
        assert result['fits_surface'] and result['coordinate_roundoff_m']==tolerance
        for penetration in (tolerance*4,1e-4):
            narrowed=shapely.box(offset-3,offset-half+penetration,offset+15,offset+half)
            assert not published_roadbed_body_check(narrowed,[piece],vehicle,.025)['fits_surface']
        obstacle=surface.difference(shapely.box(offset+4.99,offset-.01,offset+5.01,offset+.01))
        assert not published_roadbed_body_check(obstacle,[piece],vehicle,.025)['fits_surface']
        try:
            published_roadbed_body_check(surface,[piece],vehicle,tolerance)
        except ValueError:
            pass
        else:
            raise AssertionError('unrepresentable geometric error accepted')
    return dict(mesh_roundoff_reported=True,real_penetration_rejected=True)


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
                fitted_roadbed_budgets_checked=True,interior_obstruction_rejected=True,
                **verify_mesh_roundoff(vehicle),**verify_floor_coverage())
