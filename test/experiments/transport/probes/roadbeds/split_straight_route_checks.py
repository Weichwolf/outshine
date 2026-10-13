"""Check complete split routes, constant straight costs and blocked interior rejection."""

import numpy as np
import shapely

from split_straight_route_probe import split_controls
from steering_roadbed_candidate_probe import candidate
from transport_fitted_roadbeds import spline_roadbed
from transport_spline_routes import checked_spline_path


def verify():
    vehicle=dict(widthM=1.8,frontFromRearAxleM=3.3,rearOverhangM=1.1,wheelbaseM=2.7,minimumRearAxleRadiusM=4.)
    costs=[]
    for length,angle,offset in ((200.,0.,[0.,0.]),(2000.,.7,[1000.,-2000.])):
        matrix=np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]])
        transform=lambda xy:xy@matrix.T+offset
        entry=dict(position=transform(np.array([0.,0.])),heading=matrix[:,0])
        exit=dict(position=transform(np.array([length,0.])),heading=matrix[:,0])
        source=shapely.LineString([entry['position'],exit['position']])
        floor=shapely.transform(shapely.box(-10,-5,length+10,5),transform)
        controls,note=split_controls(entry,exit,source,floor,vehicle,.025,16.,8.,1024)
        assert controls is not None,note
        path,proof=checked_spline_path(controls,floor,vehicle,.025)
        assert path is not None,proof
        assert abs(path.length-length)<1e-7
        assert np.allclose(controls[0,0],entry['position'],atol=1e-7,rtol=0)
        assert np.allclose(controls[-1,-1],exit['position'],atol=1e-7,rtol=0)
        support,audit=spline_roadbed(controls,vehicle,.025)
        assert support is not None,audit
        costs.append((len(controls),audit['hull_intervals']))
        assert note['residual_calls']<=1024
        assert sum(t['residual_calls'] for t in note['terminal_fits'])==note['residual_calls']
        blocked=shapely.transform(shapely.box(length*.5-1,-2,length*.5+1,2),transform)
        data=dict(entry=entry,exit=exit,vehicle=vehicle,error_m=.025,surface=floor.difference(blocked).wkb.hex(),
                  reservations=blocked.wkb.hex(),source_route=source.wkb.hex())
        try:candidate(data,dict(curve_controls=controls))
        except AssertionError:pass
        else:raise AssertionError('unchecked straight crossed reserved interior')
        for head,tail,budget in ((0.,8.,1024),(length,8.,1024),(16.,8.,1)):
            curve,failed=split_controls(entry,exit,source,floor,vehicle,.025,head,tail,budget)
            assert curve is None and failed['residual_calls']==0
    assert costs[0]==costs[1] and costs[0][0]<64 and costs[0][1]<255
    return dict(complete_vehicle_curve_checked=True,rigid_transform=True,straight_length_independent_cost=True,
                blocked_interior_rejected=True,shared_call_budget=True,invalid_split_rejected=True,costs=costs)


if __name__=='__main__':print(verify())
