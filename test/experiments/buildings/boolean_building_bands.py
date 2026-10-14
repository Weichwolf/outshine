"""Construct the complete structural shell from planned bands, then remove internal caps."""

import shapely

from building_vertical import level
from building_plan import wall_at
from outshine_ref.buildings.modelling import PrismVolume,union_prisms
from verify_boolean_surfaces import Prism


def bands(plan):
    if plan.get('vertical_status')!='connected-storeys':raise ValueError('Bands require a complete existing plan')
    hole=plan['stair']['opening'] if 'stair' in plan else shapely.Polygon();result=[]
    for index in range(plan['storeys']):
        current=level(plan,index);prefix=plan['id']+f'/floor-{index}'
        current=current|dict(walls=current['footprint'].difference(shapely.union_all(current['rooms'])))
        cut=current['footprint'].difference(hole)
        result.append((prefix+'/floor',Prism(cut if index else current['footprint'],current['base_z_m'],current['floor_z_m']),0))
        high=level(plan,index+1)['base_z_m'] if index<plan['storeys']-1 else plan['body_top_m']
        result.append((prefix+'/ceiling',Prism(cut if index<plan['storeys']-1 else current['footprint'],current['ceiling_z_m'],high),2))
        limits={current['floor_z_m'],current['ceiling_z_m']}
        limits.update(current['floor_z_m']+door['height_m'] for door in current['doors'])
        limits.update(current['floor_z_m']+window[key] for window in current['windows'] for key in ('sill_m','head_m'))
        heights=sorted(value for value in limits if current['floor_z_m']<=value<=current['ceiling_z_m'])
        for number,(low,high) in enumerate(zip(heights[:-1],heights[1:],strict=True)):
            result.append((prefix+f'/walls-{number}',Prism(wall_at(current,(low+high)/2),low,high),1))
    return result


def construct(plan):
    return union_prisms(PrismVolume(source,prism.polygon,prism.low,prism.high,material)
        for source,prism,material in bands(plan))
