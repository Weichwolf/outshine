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


def return_court_domain(source,vehicle):
    points=shapely.get_coordinates(source)
    assert len(points)>=3,'return court needs a turning source'
    directions=np.diff(points,axis=0)
    lengths=np.linalg.norm(directions,axis=1)
    assert np.all(lengths>0),'return court source has repeated points'
    assert directions[0]@directions[-1]<=0,'return court needs opposing approaches'
    reach=2*vehicle['minimumRearAxleRadiusM']+max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])+vehicle['widthM']*.5
    assert np.linalg.norm(points[-1]-points[0])<=reach,'return court approaches are not local'
    return source.envelope.buffer(reach),reach


def loop_court_support(layout,surface,vehicle,error,recipe='loop-court'):
    assert recipe in ('loop-court','return-court'),'unknown court recipe'
    assert 0<len(layout)<=8,'loop layout exceeds its bounded curve budget'
    domains=[];supports=[];intervals=0
    for item in layout:
        source=shapely.from_wkb(item['source'])
        domain,reach=loop_court_domain(surface,source,vehicle) if recipe=='loop-court' else return_court_domain(source,vehicle)
        support,audit=spline_roadbed(np.array(item['controls']),vehicle,error)
        assert support is not None,audit
        domains.append(domain);supports.append(support);intervals+=audit['hull_intervals']
    return shapely.union_all(supports),shapely.union_all(domains),dict(
        recipe=recipe,source_reach_m=reach,layout_curves=len(layout),hull_intervals=intervals)
