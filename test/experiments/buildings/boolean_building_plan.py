"""Reuse complete room/storey plans as explicit closed building CSG inputs."""

from dataclasses import replace

import numpy as np
import shapely

from boolean_sequence import leaf,combine
from building_layout import polygons
from building_vertical import level
from outshine_ref.buildings.modelling import Shape
from verify_boolean_attributes import authored
from verify_boolean_surfaces import Prism


def sources(plan):
    if plan.get('vertical_status')!='connected-storeys':
        raise ValueError('Building CSG requires a complete existing storey plan')
    result={'outer':Prism(plan['footprint'],plan['base_z_m'],plan['body_top_m'])}
    for index in range(plan['storeys']):
        current=level(plan,index);prefix=f'floor-{index}'
        result[prefix+'/rooms']=Prism(shapely.union_all(current['rooms']),current['floor_z_m'],current['ceiling_z_m'])
        for number,door in enumerate(current['doors']):
            result[prefix+f'/door-{number}']=Prism(door['polygon'],current['floor_z_m'],current['floor_z_m']+door['height_m'])
        for number,window in enumerate(current['windows']):
            result[prefix+f'/window-{number}']=Prism(window['polygon'],current['floor_z_m']+window['sill_m'],
                current['floor_z_m']+window['head_m'])
        if index<plan['storeys']-1 and 'stair' in plan:
            result[prefix+'/stair-opening']=Prism(plan['stair']['opening'],current['ceiling_z_m'],level(plan,index+1)['floor_z_m'])
    return result


def composite(name,prism,owner):
    parts=[authored(Prism(polygon,prism.low,prism.high).shape(),owner) for polygon in polygons(prism.polygon)]
    if not parts:raise ValueError('Declared building source has no positive polygon component')
    vertices=[];faces=[];offset=0;groups={}
    for number,shape in enumerate(parts):
        points=shape.emit()['vertices'];vertices.append(points);groups[number]=name+f'/component-{number}'
        for face in shape.geometry.polygons:
            faces.append(replace(face,outer=tuple(i+offset for i in face.outer),
                holes=tuple(tuple(i+offset for i in ring) for ring in face.holes),group=number,
                tags=face.tags|{'leaf.'+name}))
        offset+=len(points)
    shape=Shape.from_faces(np.concatenate(vertices),faces).with_metadata(group_names=groups,
        material_defs=parts[0].geometry.material_defs)
    return leaf(name,replace(shape,material=parts[0].material,textures=parts[0].textures))


def unite(entries):
    if len(entries)==1:return entries[0]
    middle=len(entries)//2
    first,left=unite(entries[:middle]);last,right=unite(entries[middle:])
    identifier='cutters/'+first.leaves[0].identifier+'/'+last.leaves[-1].identifier
    combined=combine(identifier,first,last)
    return combined.operand,('union',left,right)


def construct(prisms,strategy):
    declared={name:composite(name,prism,int(name!='outer')) for name,prism in prisms.items()}
    entries=[(value,name) for name,value in declared.items() if name!='outer']
    if strategy=='balanced-cutters':
        cutter,expression=unite(entries)
        final=combine('building-shell',declared['outer'],cutter,'subtract')
        expression=('subtract','outer',expression)
    elif strategy=='sequential-cuts':
        current=declared['outer'];expression='outer'
        for cutter,name in entries:
            final=combine('shell-after/'+name,current,cutter,'subtract');current=final.operand
            expression=('subtract',expression,name)
    else:raise ValueError('Unknown building CSG strategy')
    return declared,final,expression
