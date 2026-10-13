"""Nested storey envelopes retaining the existing core, common access and room identities."""

import math

import shapely

from building_layout import AGENT_RADIUS_M,WALL_M,canonical_geometry,polygons
from building_plan import walkable,windows
from building_vertical import level


def candidate(source,footprint):
    if footprint.equals(source['footprint']):return source,None
    interior=footprint.buffer(-WALL_M,join_style='mitre')
    rooms=[canonical_geometry(room.intersection(interior)) for room in source['rooms']]
    if any(room.geom_type!='Polygon' or room.is_empty or room.area<5. for room in rooms):
        return None,'setback leaves an empty, disconnected or undersized room'
    current=source|dict(footprint=footprint,rooms=rooms,
        walls=canonical_geometry(footprint.difference(shapely.union_all(rooms))))
    current['windows']=windows(footprint,rooms,current['doors'])
    free=walkable(current)
    reached=next((part for part in polygons(free) if part.covers(rooms[0].representative_point())),None)
    if reached is None or any(room.buffer(-AGENT_RADIUS_M,join_style='mitre').is_empty or
            room.buffer(-AGENT_RADIUS_M,join_style='mitre').difference(reached).area>1e-6 for room in rooms):
        return None,'setback breaks common access or room clearance'
    current['walkable']=free
    return current,None


def apply(plan,offsets):
    if plan.get('vertical_status')!='connected-storeys':raise ValueError('Setbacks require a connected storey plan')
    offsets=tuple(offsets)
    if len(offsets)!=plan['storeys'] or not offsets or offsets[0]!=0:
        raise ValueError('One offset per storey is required, with an unchanged ground floor')
    if any(isinstance(value,bool) or not math.isfinite(value) or value<0 for value in offsets):
        raise ValueError('Setbacks require finite nonnegative metre distances')
    if any(b<a for a,b in zip(offsets,offsets[1:])):raise ValueError('Setbacks must be nested')
    if 'storey_plans' in plan:raise ValueError('Apply setbacks to the original fixed envelope')
    reserve=shapely.union_all([plan['core_envelope'],plan['corridor_zone']]).buffer(WALL_M,join_style='mitre')
    reserve=canonical_geometry(reserve.intersection(plan['footprint']))
    floors=[];decisions=[];previous=plan['footprint']
    for index,distance in enumerate(offsets):
        source=level(plan,index)
        outline=canonical_geometry(plan['footprint'].buffer(-distance,join_style='mitre').union(reserve).intersection(previous)) if distance else previous
        current,reason=None,None
        if outline.geom_type=='Polygon' and not outline.is_empty and outline.is_valid:
            current,reason=candidate(source,outline)
        else:reason='setback envelope is empty or disconnected'
        if current is None:
            current,baseline_reason=candidate(source,previous)
            if current is None:raise ValueError('Retained envelope is unusable: '+str(baseline_reason))
        terrace=canonical_geometry(previous.difference(current['footprint']))
        decisions.append(dict(storey=index,requested_offset_m=distance,reason=reason,
            footprint_area_m2=current['footprint'].area,terrace_area_m2=terrace.area))
        floors.append(current|dict(terrace=terrace));previous=current['footprint']
    return plan|dict(storey_plans=tuple(floors),setback_decisions=decisions,
        scope='Nested storeys with retained circulation; terrace access and guards not yet constructed')
