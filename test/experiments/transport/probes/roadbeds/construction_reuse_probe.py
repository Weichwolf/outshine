"""Compare complete roadbed construction on frozen real road requests."""

import argparse
import json
from pathlib import Path
import resource
import subprocess
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import transport_fitted_roadbeds as fitted
import transport_source_roadbeds as source
from transport_cut_setback_experiment import prepared_network,implementation_files,digest
from transport_cut_setbacks import restore_cut_plan
from transport_directed_routes import directed_module_routes
from transport_junction_surfaces import motor_surface
from transport_turn_roadbed_experiment import building_reservations,mesh_coverage
from transport_travel import MODES
from hermite_candidate_reuse_probe import reuse_candidates


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--place',required=True)
    parser.add_argument('--cases',nargs='+',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();root=Path('build/experiments/transport')
    before=json.loads((root/'window-1000-body'/f'{args.place}-routes.json').read_text())
    frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
    network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==args.place))
    restore_cut_plan(network,json.loads((root/'cut-setbacks'/f'{args.place}-cut-setbacks.json').read_text())['plan'])
    bounds=shapely.MultiPoint(network.product['vertices']).envelope.buffer(10)
    receipt=json.loads((root/'inputs'/f'{args.place}-input.json').read_text())['receipt']
    buildings,_=building_reservations(receipt,bounds)
    nonmotor=~np.any(network.graph['travel_public']&MODES['motor']!=0,axis=1)
    other=shapely.union_all(shapely.buffer(network.graph['lines'][nonmotor],network.graph['half_widths'][nonmotor],quad_segs=2))
    reserved=buildings.union(other)
    baseline=fitted.fitted_route_candidates;rows=[]
    args.output.mkdir(parents=True,exist_ok=True)
    for case in args.cases:
        module,*pair=map(int,case.split(':'));assert len(pair)==2
        poses,_=network.poses(module,{})
        shape=network.shape(module);error=network.curbs['arc_error_m']
        floor=motor_surface(shape,network.graph,network.tree,network.mesh,network.vehicle,
                            network.curbs['corner_radius_m'],error)
        routes,_=directed_module_routes(network.graph,poses,shape,network.tree)
        entry,exit=(poses[i] for i in pair);route=routes[tuple(pair)]
        snapshot=dict(entry={k:entry[k].tolist() for k in ('position','heading')},
                      exit={k:exit[k].tolist() for k in ('position','heading')},
                      surface=floor.wkb.hex(),reservations=reserved.wkb.hex(),source_route=route.wkb.hex(),
                      vehicle=network.vehicle,error_m=error)
        path=args.output/f'{case.replace(":","-")}-input.json'
        path.write_text(json.dumps(snapshot)+'\n')
        previous=None
        try:
            for name,method in (('baseline',baseline),('reuse',reuse_candidates)):
                fitted.fitted_route_candidates=method
                began=time.perf_counter();cpu=resource.getrusage(resource.RUSAGE_SELF)
                candidate,trials=source.construct_route_roadbed(entry,exit,floor,reserved,route,network.vehicle,error)
                measured=resource.getrusage(resource.RUSAGE_SELF)
                row=dict(module=module,pair=pair,method=name,accepted=candidate is not None,
                         elapsed_ms=(time.perf_counter()-began)*1000,
                         cpu_ms=(measured.ru_utime+measured.ru_stime-cpu.ru_utime-cpu.ru_stime)*1000,
                         fitting_calls=sum(t.get('residual_calls',0) for t in trials),trials=trials)
                if candidate is not None:
                    mesh,_=mesh_coverage(candidate['surface'],network.vehicle,candidate['pieces'],error)
                    row.update(mesh=mesh,added_area_m2=candidate['addition'].area,
                               path_wkb=candidate['path'].wkb.hex())
                if name=='baseline':previous=row
                else:
                    assert not previous['accepted'] or row['accepted'],'previous valid construction lost'
                    row['previous_path_preserved']=not previous['accepted'] or row['path_wkb']==previous['path_wkb']
                rows.append(row)
                print(json.dumps({k:row[k] for k in ('module','pair','method','accepted','elapsed_ms','cpu_ms','fitting_calls')}),flush=True)
        finally:fitted.fitted_route_candidates=baseline
    sources=implementation_files(Path(__file__))+implementation_files(Path(source.__file__))
    sources+=implementation_files(Path(prepared_network.__code__.co_filename))
    sources+=implementation_files(Path(mesh_coverage.__code__.co_filename))
    sources+=implementation_files(Path(reuse_candidates.__code__.co_filename))
    report=dict(place=args.place,rows=rows,input_sha256={str(p):digest(p) for p in args.output.glob('*-input.json')},
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources},
        scope='Individual complete constructions and their actual triangle meshes; shared ownership and full network remain open.')
    (args.output/'results.json').write_text(json.dumps(report,indent=2,default=lambda value:value.tolist())+'\n')


if __name__=='__main__':main()
