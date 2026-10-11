"""Find locally feasible corridor headings before solving the neighboring modules."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import infrastructure_coupled_roadbed_experiment as coupled
from infrastructure_cut_setback_experiment import prepared_network
from infrastructure_cut_setbacks import restore_cut_plan
from infrastructure_coupled_roadbed_products import apply_coupled_product
from infrastructure_directed_routes import directed_module_routes
from infrastructure_junction_surfaces import motor_surface
from infrastructure_shared_frames import frame_key,heading_choices
from infrastructure_turn_roadbed_experiment import building_reservations,reservations


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prior',type=Path,required=True)
    parser.add_argument('--module',type=int,required=True)
    parser.add_argument('--pair',type=int,nargs=2,required=True)
    parser.add_argument('--both-ports',action='store_true')
    parser.add_argument('--maximum-candidates',type=int,default=64)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.maximum_candidates<1:raise ValueError('candidate budget must be positive')
    root=Path('build/experiments/infrastructure');prior=json.loads(args.prior.read_text());name=prior['place']
    before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
    reference=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
    network=prepared_network(root,before,next(p['frames'] for p in reference['places'] if p['place']==name))
    cuts=json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text());restore_cut_plan(network,cuts['plan'])
    records={r['module']:r for r in before['modules']};records.update((r['module'],r) for r in cuts['changed_modules'])
    network,records,_,_=apply_coupled_product(network,records,set(),root,args.prior,set())
    poses,_=network.poses(args.module,{})
    ports=[poses[i] for i in args.pair]
    owners=set.union(*(network.owners[frame_key(p)] for p in ports))
    cooked={s['module']:shapely.from_wkb(s['public_surface_wkb']) for s in prior['published_surfaces']}
    floor=lambda owner:motor_surface(network.shape(owner),network.graph,network.tree,network.mesh,network.vehicle,
                                    network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
    floors={owner:cooked[owner] if owner in cooked else floor(owner) for owner in owners}
    choices=[heading_choices(p,network.anchors[frame_key(p)],[floors[k] for k in network.owners[frame_key(p)]],
                             network.vehicle,network.curbs['arc_error_m']) for p in ports]
    if not args.both_ports:choices[0]=[(ports[0]['heading'],0.)]
    options=sorted(((a,b) for a in choices[0] for b in choices[1]),key=lambda pair:sum(p[1] for p in pair))
    routes,_=directed_module_routes(network.graph,poses,network.shape(args.module),network.tree)
    route=routes[tuple(args.pair)]
    base=motor_surface(route.envelope,network.graph,network.tree,network.mesh,network.vehicle,
                       network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
    region=shapely.union_all([network.shape(owner) for owner in owners]).envelope.buffer(coupled.roadbed_lateral_reach(network.vehicle))
    receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
    buildings,_=building_reservations(receipt,region)
    reserved=buildings.union(reservations(network,region).difference(base))
    cache=dict(contexts={},hits=0,misses=0);trials=[];winner=None;begin=time.perf_counter()
    for selected in options[:args.maximum_candidates]:
        moved=[dict(p,heading=h.copy()) for p,(h,_) in zip(ports,selected)]
        candidate,existing,notes=coupled.roadbed_candidate(*moved,route,base,reserved,network.vehicle,
                                    network.curbs['arc_error_m'],False,cache)
        row=dict(headings=[p['heading'].tolist() for p in moved],angular_cost_rad=sum(h[1] for h in selected),
                 accepted=candidate is not None,reasons=[n.get('reason') for n in notes])
        trials.append(row);print(json.dumps(row),flush=True)
        if candidate is not None:
            winner=dict(frames=[{k:p[k] for k in ('section','edge','direction','station','position','heading')} for p in moved],
                        candidate=candidate,existing_surface=existing)
            break
    args.output.mkdir(parents=True,exist_ok=True)
    sources=coupled.implementation_files(Path(__file__))
    report=dict(place=name,module=args.module,pair=args.pair,owners=sorted(owners),trials=trials,winner=winner,
                candidate_budget=args.maximum_candidates,elapsed_ms=(time.perf_counter()-begin)*1000,
                input_sha256={str(args.prior):coupled.digest(args.prior)},
                measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
                implementation_sha256={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in sources},
                scope='One corridor curve and static bodies on both neighboring floors; coupled publication remains required.')
    def encode(value):
        if isinstance(value,shapely.Geometry):return value.wkb.hex()
        if isinstance(value,np.ndarray):return value.tolist()
        if isinstance(value,np.generic):return value.item()
        raise TypeError(type(value).__name__)
    (args.output/'results.json').write_text(json.dumps(report,indent=2,default=encode)+'\n')
    print(json.dumps(dict(found=winner is not None,trials=len(trials),elapsed_ms=report['elapsed_ms'])),flush=True)


if __name__=='__main__':main()
