"""Real rejected one-floor sources, actual door access and unchanged successful plans."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely

import building_circulation
import building_layout
from boolean_building_bands import construct
from boolean_building_plan import sources
from building_setbacks import apply as setbacks
from building_terraces import apply as terraces
from building_vertical import build
from building_plan_cases import cases as layout_cases
from evidence import revision
from interior_light_plan import lights
from structural_shell_geometry import assembly
from tile_buildings import read_tile
from tile_room_details import numeric_properties
from verify_room_fallback import baseline_module,blocked_entry,cases as previous_cases,equal_plan
from verify_structural_shell import geometry,access
from verify_vertical import connected_spaces


INPUTS=Path(__file__).resolve().parents[3]/'doc/references/data/osm/single-storey-study-20261014.json'


def cases():
    inventory=json.loads(INPUTS.read_text());loaded={}
    for item in inventory['cases']:
        if item['site'] not in loaded:loaded[item['site']]=read_tile(item['site'])
        rows,receipt=loaded[item['site']];row=next(r for r in rows if r['id']==item['id'])
        yield dict(id=row['id'],polygon=row['polygon'],properties=numeric_properties(row),
                   source_sha256=receipt['receipt']['sha256'])


def previous_plan(sample,vertical,layout):
    original=building_layout.room_portals,building_circulation.room_portals
    building_layout.room_portals=building_circulation.room_portals=layout.room_portals
    try:return vertical.build(sample)
    finally:building_layout.room_portals,building_circulation.room_portals=original


def timed(function,*args,**kwargs):
    began=time.perf_counter();value=function(*args,**kwargs)
    return value,(time.perf_counter()-began)*1000


def recovered(sample,before,plan,elapsed):
    if plan['vertical_status']!='connected-storeys' or plan['storeys']!=1:
        raise ValueError('Previously rejected single-storey source remains unplanned')
    if 'stair' in plan or plan['vertical_portals']:raise ValueError('Single-storey source acquired fictional stairs')
    direct=plan['circulation_status']=='direct-rooms'
    if direct and ('core_envelope' in plan or 'corridor_zone' in plan):
        raise ValueError('Direct rooms carry an unnecessary reserved core')
    if len(plan['circulation_alternatives'])>12:raise ValueError('Routing budget increased')
    shell,shell_ms=timed(construct,plan);(mesh,collision),detail_ms=timed(assembly,plan,shell,frames=True)
    shape_proof=geometry(plan,sources(plan),shell);actor_proof,actor_ms=timed(access,plan,collision)
    equal_plan(plan,build(sample))
    states={name:lights(plan,name) for name in ('all-lit','mixed','off')}
    ids={v for floor in plan['storey_room_ids'] for v in floor}
    for state in states.values():
        if {lamp['room_id'] for lamp in state}!=ids:raise ValueError('Night lights changed the room world')
    if any(lamp['enabled'] for lamp in states['off']):raise ValueError('Off room state remains lit')
    if direct and not all(lamp['enabled'] for lamp in states['all-lit']):
        raise ValueError('Direct rooms inherited an unlit service reservation')
    if not blocked_entry(plan,collision):raise ValueError('Sealed entry was not detected')
    zero=terraces(setbacks(plan,[0.]));equal_plan(plan,zero)
    if zero['terrace_access']:raise ValueError('An unchanged single storey acquired terrace access')
    before_reason=None
    if before['vertical_status']=='connected-storeys':
        _,previous_collision=assembly(before,construct(before),frames=True)
        try:access(before,previous_collision)
        except ValueError as error:before_reason=str(error)
        else:raise ValueError('The declared blocked baseline source had valid access')
    return dict(id=sample['id'],source_sha256=sample['source_sha256'],mode=plan['circulation_status'],
        before_status=before['vertical_status'],before_access_rejection=before_reason,
        plan_ms=elapsed,shell_ms=shell_ms,detail_ms=detail_ms,actor_check_ms=actor_ms,spaces=connected_spaces(plan),
        triangles=len(mesh['faces']),geometry=shape_proof,access=actor_proof,room_ids=plan['storey_room_ids'],light_states=states)


def compatibility(old_vertical,old_layout):
    results=[]
    for sample in [*previous_cases(),next(s for s in layout_cases() if s['label']=='tokyo-compact')]:
        before=previous_plan(sample,old_vertical,old_layout);after=build(sample)
        equal_plan(before,after)
        row=dict(id=sample['id'],source_sha256=sample['source_sha256'],previous_geometry_byte_equal=True)
        if sample.get('label')=='tokyo-compact':
            _,collision=assembly(after,construct(after),frames=True);row['access']=access(after,collision)
        results.append(row)
    if len(results)!=13:raise ValueError('Previous successful source set changed')
    return results


def benchmarks(sample,old_vertical,old_layout,repeats):
    values={name:[] for name in ('previous-plan','current-plan','shell','details')}
    for index in range(repeats+2):
        before,previous_ms=timed(previous_plan,sample,old_vertical,old_layout)
        plan,plan_ms=timed(build,sample);shell,shell_ms=timed(construct,plan)
        _,detail_ms=timed(assembly,plan,shell,frames=True)
        if index>=2:
            for name,value in zip(values,(previous_ms,plan_ms,shell_ms,detail_ms),strict=True):values[name].append(value)
    return dict(id=sample['id'],previous_plan_valid=before['vertical_status']=='connected-storeys',
        phases={name:dict(p50_ms=float(np.percentile(data,50)),p95_ms=float(np.percentile(data,95)),samples=repeats)
                for name,data in values.items()},
        scope='Sequential warm Python build phases on this host; actual access checked separately, no native frame/device timing')


def run(output,commit,repeats):
    old_vertical,vertical_sha=baseline_module(commit,'building_vertical');old_layout,layout_sha=baseline_module(commit,'building_layout')
    samples=list(cases());results=[];negative=[]
    for sample in samples:
        before=previous_plan(sample,old_vertical,old_layout);plan,elapsed=timed(build,sample)
        if plan['storeys']!=1:
            if plan.get('circulation_status')=='direct-rooms':raise ValueError('Multiple storeys used a first-floor fallback')
            negative.append(dict(id=sample['id'],storeys=plan['storeys'],status=plan['vertical_status'],omissions=plan['omissions']));continue
        results.append(recovered(sample,before,plan,elapsed))
        print(json.dumps(dict(id=sample['id'],mode=plan['circulation_status'],status='pass')),flush=True)
    if len(results)!=7 or sum(r['mode']=='direct-rooms' for r in results)!=6 or len(negative)!=5:
        raise ValueError('Pinned single-storey recovery set changed')
    for name,polygon,properties in [('thin',shapely.box(0,0,10,1.05),{'render_height':3}),
        ('low',shapely.box(0,0,8,7),{'render_height':1.8}),
        ('two-storeys',shapely.box(0,0,14,10),{'render_height':6,'building:levels':2})]:
        plan=build(dict(id=name,polygon=polygon,properties=properties))
        if plan.get('circulation_status')=='direct-rooms':raise ValueError('Ineligible case used the single-storey fallback')
    output.mkdir(parents=True,exist_ok=True)
    comparison=compatibility(old_vertical,old_layout);timings=[]
    if repeats:
        for sample in (samples[0],next(previous_cases())):
            timings.append(benchmarks(sample,old_vertical,old_layout,repeats))
    report=revision()|dict(status='pass',baseline_commit=commit,baseline_vertical_sha256=vertical_sha,
        baseline_layout_sha256=layout_sha,input_sha256=hashlib.sha256(INPUTS.read_bytes()).hexdigest(),cases=results,
        unchanged_cases=comparison,upper_storey_controls=negative,timings=timings,
        scope='Seven actual single-storey recoveries, exact structure/replay, sampled actor access and shared light IDs; generic use, no native controller/device admission')
    (output/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/single-storey'))
    parser.add_argument('--baseline',default='64c5d840ca57cd98d4ba47345ce9d4b911fcb7e7');parser.add_argument('--repeats',type=int,default=21)
    args=parser.parse_args();run(args.output,args.baseline,args.repeats)
