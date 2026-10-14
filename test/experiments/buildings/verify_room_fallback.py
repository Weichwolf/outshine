"""Actual provider plans, equal-output baseline, independent shells and sampled actor access."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import time

import numpy as np
import shapely

import building_vertical as vertical
from boolean_building_bands import construct
from boolean_building_plan import sources
from evidence import revision
from structural_shell_geometry import assembly,native
from tile_buildings import read_tile
from tile_room_details import numeric_properties
from verify_structural_shell import geometry,access
from outshine_ref.buildings.modelling import PrismVolume,union_prisms
import boolean_building_bands as building_bands


INPUTS=Path(__file__).resolve().parents[3]/'doc/references/data/osm/room-plan-study-20261014.json'


def cases():
    inputs=json.loads(INPUTS.read_text());loaded={}
    for item in inputs['cases']:
        if item['site'] not in loaded:loaded[item['site']]=read_tile(item['site'])
        rows,receipt=loaded[item['site']];row=next(r for r in rows if r['id']==item['id'])
        yield dict(id=row['id'],polygon=row['polygon'],properties=numeric_properties(row),
                   source_sha256=receipt['receipt']['sha256'])


def baseline_module(commit,name):
    data=subprocess.check_output(['git','show',commit+':test/experiments/buildings/'+name+'.py'])
    path=Path(tempfile.gettempdir())/('outshine-buildings-'+name+'-'+commit+'.py');path.write_bytes(data)
    spec=importlib.util.spec_from_file_location(name+'_baseline',path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module,hashlib.sha256(data).hexdigest()


def baseline_kernel(commit):
    data=subprocess.check_output(['git','show',commit+':lib/src/outshine_ref/buildings/modelling/prism_union.py'])
    path=Path(tempfile.gettempdir())/('outshine-buildings-prism_union-'+commit+'.py');path.write_bytes(data)
    spec=importlib.util.spec_from_file_location('outshine_ref.buildings.modelling.prism_union_baseline',path)
    module=importlib.util.module_from_spec(spec)
    import sys
    sys.modules[spec.name]=module;spec.loader.exec_module(module)
    return lambda values:module.union_prisms(module.PrismVolume(v.identifier,v.polygon,v.low_m,v.high_m,v.material) for v in values),hashlib.sha256(data).hexdigest()


def shell_with(plan,procedure):
    previous=building_bands.union_prisms;building_bands.union_prisms=procedure;start=time.perf_counter()
    try:shell=building_bands.construct(plan)
    finally:building_bands.union_prisms=previous
    return shell,(time.perf_counter()-start)*1000


def produce(sample,procedure):
    old=vertical.circulation;vertical.circulation=procedure;start=time.perf_counter()
    try:plan=vertical.build(sample)
    finally:vertical.circulation=old
    return plan,(time.perf_counter()-start)*1000


def equal_plan(before,after):
    for key in ('id','storeys','source_properties','storey_room_ids','storey_portals','vertical_portals'):
        if before[key]!=after[key]:raise ValueError('Successful previous plan changed '+key)
    if [p.wkb for p in before['rooms']]!=[p.wkb for p in after['rooms']]:raise ValueError('Successful previous room geometry changed')
    old_mesh,new_mesh=vertical.mesh(before),vertical.mesh(after)
    for key in old_mesh:np.testing.assert_array_equal(old_mesh[key],new_mesh[key])


def blocked_entry(plan,collision):
    door=plan['entrance'];wall=union_prisms([PrismVolume('blocked-entrance',door['polygon'],
        plan['floor_z_m'],plan['floor_z_m']+door['height_m'],1)])
    blocked=collision+native(wall.mesh,wall.vertex_roots)
    try:access(plan,blocked)
    except ValueError:return True
    raise ValueError('Blocked source-house entrance was admitted')


def run(output,commit,repeats):
    output.mkdir(parents=True,exist_ok=True);original=vertical.circulation;module,old_sha=baseline_module(commit,'building_circulation');old=module.build
    old_sources,source_sha=baseline_module(commit,'boolean_building_plan');results=[]
    old_kernel,kernel_sha=baseline_kernel(commit)
    for sample in cases():
        before,before_ms=produce(sample,old);after,after_ms=produce(sample,original)
        row=dict(id=sample['id'],source_sha256=sample['source_sha256'],before_status=before['vertical_status'],
            after_status=after['vertical_status'],before_plan_ms=before_ms,after_plan_ms=after_ms,
            attempts=len(after.get('circulation_alternatives',[])),partition_sources=after.get('partition_sources',[]))
        if row['attempts']>12:raise ValueError('Existing twelve-attempt budget increased')
        if before['vertical_status']=='connected-storeys':equal_plan(before,after);row['previous_geometry_byte_equal']=True
        if after['vertical_status']!='connected-storeys':raise ValueError('Calibrated provider case still lacks all-storey access')
        previous_sources,declared=old_sources.sources(after),sources(after)
        for name,prism in declared.items():
            np.testing.assert_array_equal(shapely.get_coordinates(previous_sources[name].polygon),shapely.get_coordinates(prism.polygon))
            if shapely.get_precision(prism.polygon)!=0.:raise ValueError('Native source positions retain an implicit GEOS grid')
        row['source_coordinates_byte_equal']=True
        start=time.perf_counter();shell=construct(after);row['shell_ms']=(time.perf_counter()-start)*1000
        row['geometry']=geometry(after,declared,shell);mesh,collision=assembly(after,shell,frames=True);row['access']=access(after,collision)
        row.update(storeys=after['storeys'],spaces=len(after['rooms'])*after['storeys'],triangles=len(mesh['faces']))
        if len(results) in (0,4):
            other,elapsed=shell_with(after,old_kernel);comparison=dict(ms=elapsed,triangles=len(other.mesh['faces']))
            try:comparison.update(status='pass',geometry=geometry(after,declared,other))
            except ValueError as error:comparison.update(status='rejected',reason=str(error))
            row['global_kernel']=comparison
        if before['vertical_status']!='connected-storeys':
            if after['circulation']['seed_method']!='open-floor':raise ValueError('Failed plan did not exercise the new fallback')
            row['blocked_entrance_rejected']=blocked_entry(after,collision)
            replay,_=produce(sample,original);equal_plan(after,replay)
            np.savez_compressed(output/'recovered-house-shell.npz',**shell.mesh,roots=shell.vertex_roots)
        if repeats and (before['vertical_status']!='connected-storeys' or len(results)==0):
            timings={}
            for name,procedure in (('baseline',old),('bounded-fallback',original)):
                values=[produce(sample,procedure)[1] for i in range(repeats+2)][2:]
                timings[name]=dict(p50_ms=float(np.percentile(values,50)),p95_ms=float(np.percentile(values,95)),samples=repeats,
                    valid=before['vertical_status']=='connected-storeys' if name=='baseline' else True)
            row['timings']=timings
        results.append(row);print(json.dumps(dict(id=row['id'],status='pass',spaces=row['spaces'],poses=row['access']['sampled_poses'])),flush=True)
    if len(results)!=12 or sum(r.get('previous_geometry_byte_equal',False) for r in results)!=11:
        raise ValueError('The pinned twelve-source baseline changed; inspect it explicitly')
    result=revision()|dict(status='pass',baseline_commit=commit,baseline_sha256=old_sha,baseline_sources_sha256=source_sha,input_sha256=hashlib.sha256(INPUTS.read_bytes()).hexdigest(),
        baseline_kernel_sha256=kernel_sha,
        cases=results,scope='Calibrated actual source plans; previous valid geometry exact, actual shell sections and actor samples. No global family or native player/device admission.')
    (output/'verification.json').write_text(json.dumps(result,indent=2)+'\n');return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/room-fallback'))
    parser.add_argument('--baseline',default='771a54c45');parser.add_argument('--repeats',type=int,default=21);args=parser.parse_args()
    run(args.output,args.baseline,args.repeats)
