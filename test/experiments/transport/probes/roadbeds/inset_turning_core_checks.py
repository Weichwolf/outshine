"""Check source-bound inset turns, G2 joins, radius and traffic reservations."""

import json
import numpy as np
import shapely

from inset_turning_core_probe import core_candidates,attach_approaches
from continuous_turn_recipe import fit_turn
from transport_spline_routes import checked_spline_path
from transport_fitted_roadbeds import spline_roadbed
from steering_roadbed_candidate_probe import candidate


def verify():
    vehicle=dict(widthM=1.8,frontFromRearAxleM=3.3,rearOverhangM=1.1,wheelbaseM=2.7,minimumRearAxleRadiusM=4.)
    entry=dict(position=np.array([0.,0.]),heading=np.array([-1.,0.]))
    exit=dict(position=np.array([0.,-7.8]),heading=np.array([1.,0.]))
    choices=list(core_candidates(entry,exit,4.2,8.));assert choices
    begin,end,kind,seed,_=min(choices,key=lambda p:p[-1].length)
    core,note=fit_turn(begin,end,seed,4.2,kind[0],1.)
    assert core is not None,note
    controls=attach_approaches(core,entry,exit,begin,end,8.)
    assert np.array_equal(attach_approaches(core,entry,exit,begin,end,0.),core)
    assert np.allclose(controls[0,0],entry['position']) and np.allclose(controls[-1,-1],exit['position'])
    for a,b in zip(controls[:-1],controls[1:]):
        assert np.allclose(a[-1],b[0],atol=1e-9,rtol=0)
        u,v=a[-1]-a[-2],b[1]-b[0]
        assert np.allclose(u/np.linalg.norm(u),v/np.linalg.norm(v),atol=1e-8,rtol=0)
        aa,bb=a[-1]-2*a[-2]+a[-3],b[2]-2*b[1]+b[0]
        ka=(u[0]*aa[1]-u[1]*aa[0])*4/(5*np.linalg.norm(u)**3)
        kb=(v[0]*bb[1]-v[1]*bb[0])*4/(5*np.linalg.norm(v)**3)
        assert abs(ka-kb)<1e-7
    domain=shapely.box(-30,-30,10,10)
    path,proof=checked_spline_path(controls,domain,vehicle,.025)
    assert path is not None and proof['curvature_bound_per_m']<=.25
    for angle,offset in ((.7,[1000.,-2000.]),(2.1,[-80.,50.])):
        matrix=np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]])
        transform=lambda xy:xy@matrix.T+offset
        moved,note=checked_spline_path(transform(controls),shapely.transform(domain,transform),vehicle,.025)
        assert moved is not None,note
        assert abs(moved.length-path.length)<1e-7
    support,audit=spline_roadbed(controls,vehicle,.025);assert support is not None,audit
    floor=support.union(shapely.box(-10,-2,1,2))
    source=shapely.LineString([[0,0],[-12,0],[-12,-7.8],[0,-7.8]])
    values=dict(entry=entry,exit=exit,vehicle=vehicle,error_m=.025,surface=floor.wkb.hex(),
                source_route=source.wkb.hex(),reservations=shapely.Polygon().wkb.hex())
    road,_=candidate(values,dict(curve_controls=controls));assert road['path'].length>0
    forbidden=shapely.box(-30,-30,10,10).difference(shapely.box(-2,-2,2,2))
    values['surface']=floor.difference(forbidden).wkb.hex()
    values['reservations']=forbidden.wkb.hex()
    try:candidate(values,dict(curve_controls=controls))
    except AssertionError:pass
    else:raise AssertionError('inset turn crossed reserved transport')
    return dict(source_endpoints=True,g2_joins=True,radius_bound=True,rigid_transforms=True,reserved_transport_rejected=True)


if __name__=='__main__':print(json.dumps(verify()))
