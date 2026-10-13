"""Exercise source-bounded island layouts without changing vehicle or reservation rules."""

import json
import numpy as np
import shapely

from rounded_lane_loop_probe import rounded_polyline
from steering_roadbed_candidate_probe import candidate
from transport_loop_court import loop_court_domain,return_court_domain


def verify():
    vehicle=dict(widthM=1.8,frontFromRearAxleM=3.3,rearOverhangM=1.1,wheelbaseM=2.7,minimumRearAxleRadiusM=4.)
    points=np.array([[0.,0.],[20.,0.],[20.,20.],[0.,20.],[0.,3.]])
    source=shapely.LineString(points);floor=shapely.LineString(np.vstack([points,points[0]])).buffer(1.)
    controls=rounded_polyline(points,4.5,.5)
    data=dict(entry=dict(position=points[0],heading=np.array([1.,0.])),
        exit=dict(position=points[-1],heading=np.array([0.,-1.])),surface=floor.wkb.hex(),
        reservations=shapely.Polygon().wkb.hex(),source_route=source.wkb.hex(),vehicle=vehicle,error_m=.025)
    note=dict(curve_controls=controls,road_space_recipe='loop-court')
    road,_=candidate(data,note)
    assert road['proof']['curvature_bound_per_m']<=.25 and road['mesh_audit']
    domain,reach=loop_court_domain(floor,source,vehicle)
    assert np.isclose(reach,12.2) and domain.covers(road['roadbed'])
    changed=dict(data,reservations=domain.difference(floor).wkb.hex())
    try:candidate(changed,note)
    except AssertionError as error:assert 'outside_surface' in str(error),str(error)
    else:raise AssertionError('island layout crossed reserved space')
    try:loop_court_domain(floor,shapely.LineString([[0,0],[10,0],[20,0]]),vehicle)
    except AssertionError as error:assert str(error)=='source does not wind around a traffic island'
    else:raise AssertionError('straight source was treated as an island loop')
    try:candidate(data,dict(note,road_space_recipe='unknown'))
    except AssertionError as error:assert str(error)=='unknown road space recipe'
    else:raise AssertionError('unknown recipe was accepted')
    returning=shapely.LineString([[0,0],[0,10],[5,10],[5,0]])
    domain,reach=return_court_domain(returning,vehicle)
    assert np.isclose(reach,12.2) and domain.covers(returning)
    for invalid in (shapely.LineString([[0,0],[5,0],[10,0]]),
                    shapely.LineString([[0,0],[0,20],[20,20],[20,0]]),
                    shapely.LineString([[0,0],[0,0],[5,0]])):
        try:return_court_domain(invalid,vehicle)
        except AssertionError:pass
        else:raise AssertionError('invalid return court source accepted')
    return dict(source_bounded_layout=True,native_mesh_body_checked=True,radius_unchanged=True,
                reservations_preserved=True,nonloop_source_rejected=True,unknown_recipe_rejected=True,local_return_domain_bounded=True)


if __name__=='__main__':print(json.dumps(verify()))
