"""Complete connected storeys, retaining direct room access when one floor needs no core."""

import numpy as np
import shapely

from building_circulation import build as circulation
from building_layout import canonical_geometry, rectangle
from building_mesh import panes, solid
from building_plan import CEILING_M, wall_bands, windows
from building_terraces import PARAPET_HEIGHT_M
from building_stair import FLIGHT_WIDTH_M, PLATE_M, flight, guard, landing_guard


MAX_STOREYS = 64


def level(plan, index):
    if 'storey_plans' in plan:return plan['storey_plans'][index]
    shift = index*(plan['body_top_m']-plan['base_z_m'])/plan['storeys']
    doors = [door for door in plan['doors'] if index == 0 or door['kind'] != 'entrance']
    return plan | dict(base_z_m=plan['base_z_m']+shift, floor_z_m=plan['floor_z_m']+shift,
                       ceiling_z_m=plan['ceiling_z_m']+shift, doors=doors,
                       windows=windows(plan['footprint'], plan['rooms'], doors))


def build(sample, body_top_m=None):
    plan = circulation(sample, require_stairs=True,body_top_m=body_top_m)
    plan['vertical_status'] = 'unplanned'
    if plan.get('circulation_status') != 'common-access':
        if plan['storeys']!=1 or plan['status']!='connected-first-storey':return plan
        plan=plan|dict(circulation_status='direct-rooms',room_kinds=['generic-room']*len(plan['rooms']),
            circulation=dict(seed_method=plan['method'],mode='single-storey direct rooms'))
    if plan['storeys'] > MAX_STOREYS:
        plan['omissions'].append('storey count exceeds the 64-storey constructive prototype budget')
        return plan
    if plan['storeys'] > 1 and 'stair' not in plan:
        plan['omissions'].append('no contained staircase with a connected approach landing')
        return plan
    direct=plan.get('circulation_status')=='direct-rooms'
    plan.update(vertical_status='connected-storeys', version=3,
                scope='Complete single storey with directly connected generic rooms' if direct else
                      'All storeys of the fixed envelope, shared common access and constructed stairs; generic room programme',
                storey_room_ids=[[f"{plan['id']}:floor-{index}:access-{room}" for room in range(len(plan['rooms']))]
                                 for index in range(plan['storeys'])])
    if 'stair' in plan:
        plan['room_kinds'][1] = 'stairwell'
    plan['storey_portals'] = [[dict(id=f"{plan['id']}:floor-{index}:door-{number}",
                                    rooms=[plan['storey_room_ids'][index][room] for room in door['rooms']],
                                    outside=door['kind']=='entrance')
                              for number,door in enumerate(level(plan,index)['doors'])]
                             for index in range(plan['storeys'])]
    plan['vertical_portals'] = [dict(id=f"{plan['id']}:stair-{index}",
                                     rooms=[plan['storey_room_ids'][index][1],plan['storey_room_ids'][index+1][1]])
                               for index in range(plan['storeys']-1)]
    plan['omissions'] = [reason for reason in plan['omissions']
                         if 'upper storeys and vertical circulation remain unplanned' not in reason
                         and 'core envelope is reserved' not in reason]
    plan['omissions'].append('room access and geometric contacts are experimental; native player controller remains unproved' if direct else
        'stair headroom and geometric contacts are experimental; native player controller remains unproved')
    return plan


def mesh(plan, method='profile', seal_openings=False):
    storage = [], [], [], []
    face_levels = []
    hole = plan['stair']['opening'] if 'stair' in plan and not seal_openings else shapely.Polygon()
    for index in range(plan['storeys']):
        first = len(storage[1])
        current = level(plan, index)
        cut = canonical_geometry(current['footprint'].difference(hole))
        floor = cut if index else current['footprint']
        ceiling = cut if index < plan['storeys']-1 else current['footprint']
        solid(storage, floor, current['base_z_m'], current['floor_z_m'], 0)
        if 'terrace_guard' in current:
            solid(storage,current['terrace'],current['base_z_m'],current['floor_z_m'],4)
            solid(storage,current['terrace_guard'],current['floor_z_m'],current['floor_z_m']+PARAPET_HEIGHT_M,7)
        if index and 'stair' in plan:
            support = canonical_geometry(plan['rooms'][1].difference(hole))
            solid(storage, support, current['floor_z_m']-PLATE_M, current['base_z_m'], 6)
            solid(storage,landing_guard(plan['stair'],index==plan['storeys']-1),
                  current['floor_z_m'],current['floor_z_m']+1.,7)
        if index == 0:
            solid(storage, plan['entrance']['landing'].difference(plan['footprint']), current['base_z_m'], current['floor_z_m'], 4)
        for polygon, low, high in wall_bands(current):
            solid(storage, polygon, low, high, 1)
        solid(storage, ceiling, current['ceiling_z_m'], current['ceiling_z_m']+CEILING_M, 2)
        panes(storage, current)
        if index < plan['storeys']-1:
            shift = index*plan['stair']['rise_m']
            for descriptor in plan['stair']['flights']:
                current_flight = descriptor | dict(base_z_m=descriptor['base_z_m']+shift)
                flight(storage,current_flight,method)
                guard(storage,current_flight,plan['stair'])
            mid = plan['stair']['mid_z_m']+shift
            solid(storage, plan['stair']['mid_landing'], mid-PLATE_M, mid, 6)
            stair = plan['stair']
            origin,across,along = (np.asarray(stair[key]) for key in ('origin','across','along'))
            centre = origin+stair['size_m'][0]/2*across+stair['back_m']*along
            barrier = rectangle(centre,across,stair['size_m'][0]-2*FLIGHT_WIDTH_M,.02,.02)
            solid(storage,barrier,mid,mid+1.,7)
        face_levels.extend([index]*(len(storage[1])-first))
    return dict(vertices=np.asarray(storage[0],dtype=np.float64), faces=np.asarray(storage[1],dtype=np.int32),
                materials=np.asarray(storage[2],dtype=np.uint8), parts=np.asarray(storage[3],dtype=np.int32),
                levels=np.asarray(face_levels,dtype=np.uint8))
