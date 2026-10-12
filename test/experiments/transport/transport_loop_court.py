"""Bound an island loop layout in the original source frame instead of compounding flares."""

import numpy as np
import shapely

from transport_steering_guide import crossing_signature
from transport_turn_roadbeds import bounded_voids
from transport_fitted_roadbeds import spline_roadbed


def loop_court_domain(surface,source,vehicle):
    islands=bounded_voids(surface)
    markers=shapely.get_coordinates(shapely.point_on_surface(islands))
    points=shapely.get_coordinates(source)
    assert len(points)>=3 and len(markers),'loop court needs a source traffic island'
    signature=crossing_signature(np.vstack([points,points[0]]),markers)
    assert any(signature),'source does not wind around a traffic island'
    reach=2*vehicle['minimumRearAxleRadiusM']+max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])+vehicle['widthM']*.5
    return source.envelope.buffer(reach),reach


def loop_court_support(layout,surface,vehicle,error):
    assert 0<len(layout)<=8,'loop layout exceeds its bounded curve budget'
    domains=[];supports=[];intervals=0
    for item in layout:
        domain,reach=loop_court_domain(surface,shapely.from_wkb(item['source']),vehicle)
        support,audit=spline_roadbed(np.array(item['controls']),vehicle,error)
        assert support is not None,audit
        domains.append(domain);supports.append(support);intervals+=audit['hull_intervals']
    return shapely.union_all(supports),shapely.union_all(domains),dict(
        recipe='loop-court',source_reach_m=reach,layout_curves=len(layout),hull_intervals=intervals)
