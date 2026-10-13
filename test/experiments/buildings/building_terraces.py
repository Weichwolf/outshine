"""Connect inset roof terraces to rooms and construct level decks with perimeter parapets."""

import numpy as np
import shapely

from building_layout import AGENT_RADIUS_M,DOOR_M,WALL_M,canonical_geometry,polygons,rectangle
from building_plan import aperture_fits,edges,owner,wall_at,windows


PARAPET_M=.10
PARAPET_HEIGHT_M=1.10


def portal(current,terrace,guard,index):
    for a,b in sorted(edges(current['footprint']),key=lambda edge:-np.linalg.norm(edge[1]-edge[0])):
        length=np.linalg.norm(b-a)
        if length<DOOR_M+.6:continue
        tangent=(b-a)/length;normal=np.array([tangent[1],-tangent[0]])
        for fraction in (.5,.25,.75):
            centre=a+(b-a)*fraction;room=owner(current['rooms'],centre,tangent)
            if room is None or current['room_kinds'][room]=='stairwell':continue
            if not aperture_fits(current['rooms'][room],centre,tangent,DOOR_M):continue
            landing=rectangle(centre,tangent,DOOR_M,.65,0.)
            if landing.difference(terrace).area>1e-7:continue
            opening=rectangle(centre,tangent,DOOR_M,.12,WALL_M+.20)
            if any(opening.distance(door['polygon'])<.25 for door in current['doors']):continue
            door=dict(id=f'terrace-{index}',rooms=[room],polygon=opening,centre=centre.tolist(),
                tangent=tangent.tolist(),height_m=2.15,kind='terrace',origin='procedural terrace access')
            proposed=current|dict(doors=current['doors']+[door])
            floor=current['footprint'].union(terrace)
            free=floor.difference(wall_at(proposed,current['floor_z_m']+.4).union(guard))
            free=canonical_geometry(free).buffer(-AGENT_RADIUS_M,join_style='mitre')
            path=shapely.LineString([centre-normal*(WALL_M+.40),centre+normal*.35])
            if not free.buffer(1e-8).covers(path):continue
            outside=terrace.difference(guard).buffer(-AGENT_RADIUS_M,join_style='mitre')
            connected=next((part for part in polygons(free) if part.covers(path)),None)
            if connected is None or outside.difference(connected).area>1e-6:continue
            return door,np.array(path.coords)
    return None,None


def apply(plan):
    if 'storey_plans' not in plan:raise ValueError('Terraces require stepped storey plans')
    if 'terrace_access' in plan:raise ValueError('Terrace access was already constructed')
    floors=[];access=[]
    for index,current in enumerate(plan['storey_plans']):
        terrace=current['terrace'];outer=current['footprint'].union(terrace)
        guard=canonical_geometry(terrace.difference(outer.buffer(-PARAPET_M,join_style='mitre')))
        current=current|dict(doors=list(current['doors']),terrace_guard=guard)
        for number,part in enumerate(polygons(terrace)):
            door,path=portal(current,part,guard,number)
            record=dict(storey=index,component=number,area_m2=part.area,
                reason=None if door else 'no room portal with connected terrace clearance')
            if door:
                current['doors'].append(door)
                record.update(room_id=plan['storey_room_ids'][index][door['rooms'][0]],
                    door_id=f"{plan['id']}:floor-{index}:{door['id']}",path=path.tolist())
            access.append(record)
        current['windows']=windows(current['footprint'],current['rooms'],current['doors'])
        if not terrace.is_empty:
            obstacles=wall_at(current,current['floor_z_m']+.4).union(guard)
            current['walkable']=canonical_geometry(outer.difference(obstacles)).buffer(-AGENT_RADIUS_M,join_style='mitre')
        floors.append(current)
    return plan|dict(storey_plans=tuple(floors),terrace_access=access,
        scope='Connected generic storeys with tested terrace portals and solid parapets; native collision remains unproved')
