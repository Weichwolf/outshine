"""Publish a steering-seeded route with all original requests, neighbors and building cuts."""

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
from infrastructure_cut_setbacks import restore_cut_plan,serialize_cut_plan
from infrastructure_directed_routes import directed_module_routes
from infrastructure_junction_surfaces import motor_surface
from steering_roadbed_candidate_probe import candidate,source_buildings,building_cuts


def seeded_cache(floor,reserved,route,poses,pair,vehicle,error,seed,required):
    context=(floor.wkb,shapely.get_precision(floor),reserved.wkb,shapely.get_precision(reserved),
             json.dumps(vehicle,sort_keys=True),error)
    key=tuple(poses[i][k].tobytes() for i in pair for k in ('position','heading'))+(
        route.wkb,shapely.get_precision(route),coupled.request_keys(poses,[pair])<=required)
    return dict(contexts={context:{key:(seed,False,[])}},hits=0,misses=0)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--place',required=True)
    parser.add_argument('--module',type=int,required=True)
    parser.add_argument('--pair',type=int,nargs=2,required=True)
    parser.add_argument('--curve',type=Path,required=True)
    parser.add_argument('--reference',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True);begin=time.perf_counter()
    root=Path('build/experiments/infrastructure');name,owner,pair=args.place,args.module,tuple(args.pair)
    before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
    frame_path=root/'boundary-optimized/hermite-routes.json'
    frames=next(p['frames'] for p in json.loads(frame_path.read_text())['places'] if p['place']==name)
    network=coupled.prepared_network(root,before,frames)
    cut_path=root/'cut-setbacks'/f'{name}-cut-setbacks.json'
    cuts=json.loads(cut_path.read_text());restore_cut_plan(network,cuts['plan'])
    records={r['module']:r for r in before['modules']};records.update((r['module'],r) for r in cuts['changed_modules'])
    poses,_=network.poses(owner,{})
    routes,_=directed_module_routes(network.graph,poses,network.shape(owner),network.tree)
    reference=next(p for p in json.loads(args.reference.read_text())['places'] if p['place']==name)
    previous={(t['entry'],t['exit']) for t in records[owner]['turns'] if t['accepted']}
    previous.update((t['entry'],t['exit']) for t in reference['trials'] if t['module']==owner and t['accepted'])
    assert all(owner not in p['modules'] for p in reference['coupled_products']),'owner needs a joint product rebuild'
    required=coupled.request_keys(poses,previous)
    assert pair in routes and pair not in previous
    floor=motor_surface(shapely.union_all(list(routes.values())).envelope,network.graph,network.tree,
                        network.mesh,network.vehicle,network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
    region=network.shape(owner).envelope.buffer(coupled.roadbed_lateral_reach(network.vehicle)+.025)
    receipt_path=root/'inputs'/f'{name}-input.json'
    receipt=json.loads(receipt_path.read_text())['receipt'];building_sources=source_buildings(receipt,region)
    other=coupled.reservations(network,region).difference(floor)
    data=dict(surface=floor.wkb.hex(),source_route=routes[pair].wkb.hex(),reservations=other.wkb.hex(),
              entry={k:poses[pair[0]][k] for k in ('position','heading')},
              exit={k:poses[pair[1]][k] for k in ('position','heading')},
              vehicle=network.vehicle,error_m=network.curbs['arc_error_m'])
    note=json.loads(args.curve.read_text());seed,_=candidate(data,note)
    buildings,building_audit=building_cuts(building_sources,floor,seed['surface'])
    protected=shapely.union_all([r['adjusted'] for r in buildings]);reserved=protected.union(other)
    data['reservations']=reserved.wkb.hex();seed,_=candidate(data,note)
    seed['report']={k:v for k,v in note.items() if k!='curve_controls'}
    sections=shapely.STRtree(np.array(list(network.sections.values()),dtype=object))
    cache=seeded_cache(floor,reserved,routes[pair],poses,pair,network.vehicle,data['error_m'],seed,required)
    product=coupled.construct_module(network,network,owner,set(),{owner},region,protected,sections,required,cache)
    assert pair in {c['pair'] for c in product['curves']},'seed not admitted to its physical owner'
    precision=json.loads((root/'directed-surfaces'/f'{name}-flat-surfaces.json').read_text())['precision_m']
    mesh,audit,proofs,surfaces=coupled.publish_coupled_roadbeds({owner:product},network,precision)
    mesh_path=args.output/'mesh.npz';np.savez_compressed(mesh_path,**mesh)
    gained=[c for c in product['curves'] if not coupled.request_keys(poses,[c['pair']])<=required]
    report=dict(place=name,module=owner,affected=[owner],setback_m=0.,cut_plan=serialize_cut_plan(network,[]),
        changed_frame_sections=[],vehicle=network.vehicle,all_requests_preserved=True,external_frames_unchanged=True,
        all_body_curves_on_native_mesh=True,proofs=proofs,published_frames=coupled.published_frames({owner:product}),
        published_surfaces=surfaces,physical_mesh_audit=audit,physical_mesh=str(mesh_path),
        physical_mesh_sha256=coupled.digest(mesh_path),original_surface_wkb=product['base'].wkb.hex(),
        original_physical_shape_wkb=network.shape(owner).wkb.hex(),
        planned_surface_wkb=product['planned'].wkb.hex(),physical_shape_wkb=product['physical'].wkb.hex(),
        previous_accepted=len(previous),accepted=len(product['curves']),connections=len(routes),unresolved=product['unresolved'],
        trials=[coupled.diagnostic_trial(c,product,network.vehicle,poses,routes) for c in gained]+product['unresolved'],
        added_area_m2=product['physical'].difference(network.shape(owner)).area,
        original_sections=[shapely.get_coordinates(p['line']) for p in network.junctions[owner]],
        planned_sections=[shapely.get_coordinates(p['line']) for p in network.junctions[owner]],
        shared_cuts_description=f'Unchanged shared cuts; {len(proofs)}/{len(routes)} routes; road-first building cuts',
        buildings=buildings,building_audit=building_audit,
        input_sha256=before['input_sha256']|{str(p.resolve().relative_to(root.resolve())):coupled.digest(p)
            for p in (cut_path,frame_path,receipt_path,args.curve,args.reference)},
        elapsed_ms=(time.perf_counter()-begin)*1000,
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        scope='All public-motor requests of one unchanged-cut owner; source buildings adapted, native body and neighbors checked; complete windows remain required.')
    sources=coupled.implementation_files(Path(__file__))+coupled.implementation_files(Path(__file__).with_name('steering_roadbed_candidate_probe.py'))
    sources+=[Path(coupled.__file__).with_name('geos_triangulation.py').resolve(),Path(coupled.__file__).with_name('infrastructure_network_recipes.json').resolve()]
    report['implementation_sha256']={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in set(sources)}
    def encode(value):
        if isinstance(value,shapely.Geometry):return value.wkb.hex()
        if isinstance(value,np.ndarray):return value.tolist()
        if isinstance(value,np.generic):return value.item()
        raise TypeError(type(value).__name__)
    (args.output/'proof.json').write_text(json.dumps(report,indent=2,default=encode)+'\n')
    print(json.dumps(dict(accepted=report['accepted'],connections=report['connections'],
                         gained=[c['pair'] for c in gained],added_area_m2=report['added_area_m2'],elapsed_ms=report['elapsed_ms'])))


if __name__=='__main__':main()
