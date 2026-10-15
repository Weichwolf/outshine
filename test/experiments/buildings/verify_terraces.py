"""Inspect terrace door clearance, supporting decks and parapets in the actual mesh."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely

from building_setbacks import apply as setbacks
from building_layout import polygons
from building_terraces import apply,PARAPET_HEIGHT_M
from building_vertical import build,mesh
from evidence import revision
from opening_details import attach,expand,slots
from verify_layout import occupancy
from verify_openings import verify_body_openings,verify_slots
from verify_vertical import Columns,inspect_poses,landing_poses,verify


def check(plan,product):
    rows=[]
    for index,current in enumerate(plan['storey_plans']):
        if current['terrace'].is_empty:continue
        floor=current['floor_z_m']
        actual=occupancy(product,4,floor-.01,floor+.01)
        assert actual.symmetric_difference(current['terrace']).area<1e-6,'Terrace deck missing'
        actual=occupancy(product,7,floor+PARAPET_HEIGHT_M-.01,floor+PARAPET_HEIGHT_M+.01)
        assert current['terrace_guard'].difference(actual).area<1e-6,'Terrace parapet missing'
    for row in plan['terrace_access']:
        if row['reason']:continue
        current=plan['storey_plans'][row['storey']];path=np.asarray(row['path'])
        poses=landing_poses(path,current['floor_z_m'])
        headroom=inspect_poses(Columns(product,path),poses)
        rows.append(dict(storey=row['storey'],component=row['component'],poses=len(poses),headroom_m=headroom))
    return rows


def circulation(plan,product):
    for row in plan['terrace_access']:
        if row['reason']:continue
        current=plan['storey_plans'][row['storey']];floor=current['floor_z_m']
        obstacles=shapely.union_all([occupancy(product,material,floor+.4,floor+1.9) for material in (1,3,7,10,11)])
        outer=current['footprint'].union(current['terrace'])
        free=outer.difference(obstacles).buffer(-.3,join_style='mitre')
        path=shapely.LineString(row['path'])
        connected=next((part for part in shapely.get_parts(free) if part.buffer(1e-8).covers(path)),None)
        assert connected is not None,'Detail blocks terrace portal'
        components=polygons(current['terrace'])
        if row['component']>=len(components):raise AssertionError('Terrace portal refers to a missing component')
        usable=components[row['component']].difference(obstacles).buffer(-.3,join_style='mitre')
        assert usable.difference(connected).area<1e-6,'Detail disconnects terrace circulation'


def detailed(plan):
    body=mesh(plan);openings=slots(plan);detail=expand(openings);product=attach(body,detail)
    verify_slots(plan,openings,detail);verify_body_openings(openings,body);check(plan,product)
    circulation(plan,product)
    unsafe=attach(body,expand([row|dict(sill=True) for row in openings]))
    try:circulation(plan,unsafe)
    except AssertionError:pass
    else:raise AssertionError('Projecting window sills did not fail the narrow terrace control')
    assert sum(row['kind']=='terrace' for row in openings)==2
    assert any(row['kind']=='window' and not row['sill'] for row in openings)
    return dict(openings=len(openings),triangles=len(detail['faces']),terrace_paths=check(plan,product))


def controls():
    rows=[]
    for name,outline in [('rectangle',shapely.box(0,0,18,14)),
            ('wing',shapely.Polygon([(0,0),(22,0),(22,10),(12,10),(12,18),(0,18)])),
            ('courtyard',shapely.Polygon([(0,0),(26,0),(26,22),(0,22)],holes=[[(9,7),(9,15),(17,15),(17,7)]]))]:
        base=setbacks(build(dict(id=name,polygon=outline,properties={'render_height':9.,'building:levels':3})),[0.,.8,1.6])
        plan=apply(base);product=mesh(plan)
        paths=check(plan,product)
        if name=='rectangle':assert len(paths)==2,'Both terraces require access'
        else:
            assert not plan['terrace_access'] and all(row['reason'] for row in base['setback_decisions'][1:])
            np.testing.assert_array_equal(product['faces'],mesh(base)['faces'])
        verify(plan,product)
        for broken in (('deck','portal','parapet') if paths else ()):
            if broken=='portal':
                floors=[current|dict(doors=[door for door in current['doors'] if door['kind']!='terrace']) for current in plan['storey_plans']]
                bad=mesh(plan|dict(storey_plans=floors))
            else:
                selected=product['materials']!=(4 if broken=='deck' else 7)
                bad={key:value[selected] if key!='vertices' else value for key,value in product.items()}
            try:check(plan,bad)
            except AssertionError:pass
            else:raise AssertionError('Broken terrace accepted: '+broken)
        rows.append(dict(case=name,paths=paths,access=plan['terrace_access'],details=detailed(plan) if paths else None))
    narrow=apply(setbacks(build(dict(id='narrow',polygon=shapely.box(0,0,18,14),
        properties={'render_height':9.,'building:levels':3})),[0.,.2,.4]))
    assert narrow['terrace_access'] and all(row['reason'] for row in narrow['terrace_access'])
    assert not any(door['kind']=='terrace' for current in narrow['storey_plans'] for door in current['doors'])
    return rows


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/terrace-verification.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',cases=controls(),limits='Sampled door paths; native collision and architectural room programmes unproved')
    args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(dict(status='pass',cases=len(record['cases']))))
