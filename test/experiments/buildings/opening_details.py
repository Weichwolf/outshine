"""Prepare camera-independent opening slots and reuse metrical component templates."""

import numpy as np

from building_layout import DOOR_M,rectangle
from building_vertical import level
from opening_modules import template,place


def slots(building):
    if building.get('vertical_status')!='connected-storeys':
        raise ValueError('Opening details require a complete common-access storey plan')
    result = []
    for index in range(building['storeys']):
        current = level(building,index)
        room_ids = building['storey_room_ids'][index]
        for window in current['windows']:
            bank=rectangle(window['centre'],window['tangent'],1.36,.16,0.)
            sill='terrace' not in current or bank.intersection(current['terrace']).area<1e-8
            result.append(dict(id=f"{building['id']}:floor-{index}:{window['id']}",kind='window',
                               sill=sill,
                               centre=window['centre'],tangent=window['tangent'],bottom_z_m=current['floor_z_m']+window['sill_m'],
                               width_m=1.20,height_m=window['head_m']-window['sill_m'],storey=index,
                               room_ids=[room_ids[room] for room in window['rooms']],origin='existing procedural room opening'))
        for door in current['doors']:
            if door['kind'] in ('entrance','terrace'):
                result.append(dict(id=f"{building['id']}:floor-{index}:{door['id']}",kind=door['kind'],
                                   centre=door['centre'],tangent=door['tangent'],bottom_z_m=current['floor_z_m'],
                                   width_m=DOOR_M,height_m=door['height_m'],storey=index,
                                   room_ids=[room_ids[room] for room in door['rooms']],origin=door['origin']))
    return result


def expand(openings, method='ring', reuse=True):
    arrays = {key:[] for key in ('vertices','faces','materials','parts','levels','opening_indices')}
    cache = {}
    vertex_offset,part_offset = 0,0
    for index,opening in enumerate(openings):
        key = opening['kind'],opening['width_m'],opening['height_m'],method,opening.get('sill',True)
        if not reuse or key not in cache:
            cache[key] = template(*key)
        product = place(cache[key],opening['centre'],opening['tangent'],opening['bottom_z_m'])
        arrays['vertices'].append(product['vertices'])
        arrays['faces'].append(product['faces']+vertex_offset)
        arrays['materials'].append(product['materials'])
        arrays['parts'].append(product['parts']+part_offset)
        arrays['levels'].append(np.full(len(product['faces']),opening['storey'],dtype=np.uint8))
        arrays['opening_indices'].append(np.full(len(product['faces']),index,dtype=np.int32))
        vertex_offset += len(product['vertices'])
        part_offset += int(product['parts'].max())+1
    if not openings:
        return dict(vertices=np.empty((0,3)),faces=np.empty((0,3),dtype=np.int32),materials=np.empty(0,dtype=np.uint8),
                    parts=np.empty(0,dtype=np.int32),levels=np.empty(0,dtype=np.uint8),opening_indices=np.empty(0,dtype=np.int32))
    return {key:np.concatenate(value) for key,value in arrays.items()}


def attach(body, detail):
    return dict(vertices=np.r_[body['vertices'],detail['vertices']],
                faces=np.r_[body['faces'],detail['faces']+len(body['vertices'])],
                materials=np.r_[body['materials'],detail['materials']],
                parts=np.r_[body['parts'],detail['parts']+int(body['parts'].max())+1],
                levels=np.r_[body['levels'],detail['levels']],
                opening_indices=np.r_[np.full(len(body['faces']),-1,dtype=np.int32),detail['opening_indices']])
