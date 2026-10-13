"""Bind court curves to current source frames and retain complete island context."""

import json
from pathlib import Path
from tempfile import TemporaryDirectory
import numpy as np
import shapely

from coupled_turning_court_probe import approach_controls,approach_segment,selected_court_files
from transport_loop_court import loop_court_domain
from transport_spline_routes import continuous_spline_joins


def verify():
    with TemporaryDirectory(prefix="outshine-court-selection-") as temporary:
        case=Path(temporary)
        (case/"results.json").write_text(json.dumps(dict(cases=[
            dict(accepted=True,added_area_m2=1,length_m=10),
            dict(accepted=True,added_area_m2=2,length_m=12),
            dict(accepted=False)],implementation_sha256={})))
        for index in range(3):(case/f"{index}-candidate.json").write_text("{}")
        assert selected_court_files(case)[0].name=="0-candidate.json"
        assert selected_court_files(case/"1-candidate.json")[0].name=="1-candidate.json"
        try:selected_court_files(case/"2-candidate.json")
        except AssertionError:pass
        else:raise AssertionError("unproved explicit candidate accepted")
    old=[dict(position=np.array([0.,0.]),heading=np.array([1.,0.])),
         dict(position=np.array([20.,20.]),heading=np.array([0.,1.]))]
    new=[dict(position=np.array([-4.,0.]),heading=old[0]['heading']),
         dict(position=np.array([20.,24.]),heading=old[1]['heading'])]
    original=approach_segment(*old)[None]
    linked=approach_controls(dict(curve_controls=original),old,new,(0,1))
    assert len(linked)==3 and continuous_spline_joins(linked)
    direct=approach_segment(*new)[None]
    assert np.array_equal(approach_controls(dict(curve_controls=direct),old,new,(0,1)),direct)
    wrong=[new[0],dict(position=new[1]['position'],heading=np.array([-1.,0.]))]
    try:approach_controls(dict(curve_controls=direct),old,wrong,(0,1))
    except AssertionError as error:assert str(error)=='court curve tangent differs from source frame'
    else:raise AssertionError('a changed heading reused an incompatible court curve')
    floor=shapely.box(-4,-4,16,12).difference(shapely.box(-1,3.8,8,4.2))
    source=shapely.LineString([[0,0],[12,4],[0,8]])
    vehicle=dict(minimumRearAxleRadiusM=4.,frontFromRearAxleM=3.3,rearOverhangM=1.1,widthM=1.8)
    domain,_=loop_court_domain(floor,source,vehicle)
    assert domain.covers(source)
    try:loop_court_domain(floor.intersection(source.envelope),source,vehicle)
    except AssertionError as error:assert str(error)=='loop court needs a source traffic island'
    else:raise AssertionError('a clipped-open island was treated as complete topology')
    return dict(original_curve_approaches_g2=True,direct_current_frames_retained=True,
                incompatible_heading_rejected=True,complete_island_context_required=True)


if __name__=='__main__':print(json.dumps(verify()))
