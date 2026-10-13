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
import transport_coupled_roadbed_experiment as coupled
from transport_complete_roadbed_module import construct_complete_module
from transport_cut_setbacks import restore_cut_plan,serialize_cut_plan
from transport_directed_routes import directed_module_routes
from transport_junction_surfaces import motor_surface,public_pedestrian_surface
from transport_turn_roadbeds import preserves_coverage
from transport_surface_regions import polygonal_region
from steering_roadbed_candidate_probe import candidate,source_buildings,building_cuts


def seeded_cache(floor,reserved,routes,poses,seeds,vehicle,error,required):
    context=(floor.wkb,shapely.get_precision(floor),reserved.wkb,shapely.get_precision(reserved),
             json.dumps(vehicle,sort_keys=True),error)
    values={}
    for pair,seed in seeds.items():
        route=routes[pair]
        key=tuple(poses[i][k].tobytes() for i in pair for k in ('position','heading'))+(
            route.wkb,shapely.get_precision(route),coupled.request_keys(poses,[pair])<=required)
        values[key]=seed,False,[]
    return dict(contexts={context:values},hits=0,misses=0)


def previous_pairs(owner,poses,routes,reference,records,prior_path):
    previous={(t['entry'],t['exit']) for t in records[owner]['turns'] if t['accepted']}
    previous.update((t['entry'],t['exit']) for t in reference['trials'] if t['module']==owner and t['accepted'])
    products=[p for p in reference['coupled_products'] if owner in p['modules']]
    if products:
        assert len(products)==1 and prior_path is not None,'existing owner needs its complete prior product'
        binding=products[0]
        assert Path(binding['report']).resolve()==prior_path.resolve()
        assert coupled.digest(prior_path)==binding['report_sha256']
        prior=json.loads(prior_path.read_text())
        assert prior['affected']==[owner] and not prior['changed_frame_sections']
        assert not any(prior['cut_plan'].values()),'changed cuts require their complete joint rebuild'
        by_request={next(iter(coupled.request_keys(poses,[pair]))):pair for pair in routes}
        previous.update(by_request[tuple(map(tuple,p['request']))] for p in prior['proofs'])
    return previous


def make_seeds(data,poses,routes,notes):
    seeds={}
    recipes={n.get('road_space_recipe','lane-flare') for n in notes.values()}
    court=bool(recipes&{'loop-court','return-court'})
    if court:assert len(recipes)==1,'coupled court curves need one space recipe'
    if court:
        data=dict(data,loop_layout=[dict(source=routes[p].wkb.hex(),controls=n['curve_controls']) for p,n in notes.items()])
    for pair,note in notes.items():
        if court:note=dict(note,road_space_recipe=next(iter(recipes)))
        query=dict(data,source_route=routes[pair].wkb.hex(),
            entry={k:poses[pair[0]][k] for k in ('position','heading')},
            exit={k:poses[pair[1]][k] for k in ('position','heading')})
        seeds[pair],_=candidate(query,note)
        seeds[pair]['report']={k:v for k,v in note.items() if k!='curve_controls'}
    return seeds


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--place',required=True)
    parser.add_argument('--module',type=int,required=True)
    parser.add_argument('--pair',type=int,nargs=2,action='append',required=True)
    parser.add_argument('--curve',type=Path,action='append',required=True)
    parser.add_argument('--prior',type=Path)
    parser.add_argument('--close-covered-seams',action='store_true')
    parser.add_argument('--crossing-area',type=Path,nargs='*',default=[])
    parser.add_argument('--reference',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True);begin=time.perf_counter()
    if len(args.pair)!=len(args.curve) or len({tuple(p) for p in args.pair})!=len(args.pair):
        parser.error('one curve per distinct pair required')
    notes={tuple(pair):json.loads(path.read_text()) for pair,path in zip(args.pair,args.curve)}
    root=Path('build/experiments/transport');name,owner=args.place,args.module
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
    previous=previous_pairs(owner,poses,routes,reference,records,args.prior)
    required=coupled.request_keys(poses,previous)
    assert notes.keys()<=routes.keys() and notes.keys()-previous
    floor=motor_surface(shapely.union_all(list(routes.values())).envelope,network.graph,network.tree,
                        network.mesh,network.vehicle,network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
    region=network.shape(owner).envelope.buffer(coupled.roadbed_lateral_reach(network.vehicle)+.025)
    crossing=polygonal_region(shapely.union_all([shapely.from_wkb(json.loads(path.read_text())['crossing']) for path in args.crossing_area]))
    shared_crossing=crossing
    crossing_owners=set()
    if not crossing.is_empty:
        eligible,_=public_pedestrian_surface(region,network.graph,network.tree)
        assert preserves_coverage(crossing,eligible),'crossing uses private or incompatible transport'
        triangles,index=network.mesh;ids=index.query(crossing,predicate='intersects')
        source=shapely.union_all(triangles[ids]);shared_crossing=polygonal_region(crossing.intersection(source))
        areas=shapely.area(shapely.intersection(triangles[ids],shared_crossing))
        crossing_owners=set(map(int,network.product['face_owners'][ids[areas>0]]))
        floor=floor.union(shared_crossing)
    receipt_path=root/'inputs'/f'{name}-input.json'
    receipt=json.loads(receipt_path.read_text())['receipt'];building_sources=source_buildings(receipt,region)
    other=coupled.reservations(network,region).difference(floor.union(crossing))
    data=dict(surface=floor.wkb.hex(),reservations=other.wkb.hex(),
              vehicle=network.vehicle,error_m=network.curbs['arc_error_m'])
    seeds=make_seeds(data,poses,routes,notes)
    buildings,building_audit=building_cuts(building_sources,floor,shapely.union_all([s['surface'] for s in seeds.values()]))
    protected=shapely.union_all([r['adjusted'] for r in buildings]);reserved=protected.union(other)
    data['reservations']=reserved.wkb.hex();seeds=make_seeds(data,poses,routes,notes)
    sections=shapely.STRtree(np.array(list(network.sections.values()),dtype=object))
    cache=seeded_cache(floor,reserved,routes,poses,seeds,network.vehicle,data['error_m'],required)
    affected={owner}|crossing_owners;products={}
    for member in sorted(affected):
        member_required=required
        member_floor=floor
        if member!=owner:
            member_poses,_=network.poses(member,{})
            record=records[member] if member in records else network.record(member)[0]
            member_required=coupled.request_keys(member_poses,[(t['entry'],t['exit']) for t in record['turns'] if t['accepted']])
            member_floor=motor_surface(network.shape(member),network.graph,network.tree,network.mesh,
                network.vehicle,network.curbs['corner_radius_m'],network.curbs['arc_error_m']).union(shared_crossing)
        products[member]=construct_complete_module(network,network,member,set(),affected,region,protected,sections,
            member_required,cache,reserved,public_floor=member_floor if not crossing.is_empty else None,close_seams=args.close_covered_seams)
        assert preserves_coverage(network.shape(member),products[member]['physical']),'existing transport geometry lost'
    product=products[owner]
    assert seeds.keys()<={c['pair'] for c in product['curves']},'seed not admitted to its physical owner'
    precision=json.loads((root/'directed-surfaces'/f'{name}-flat-surfaces.json').read_text())['precision_m']
    mesh,audit,proofs,surfaces=coupled.publish_coupled_roadbeds(products,network,precision)
    mesh_path=args.output/'mesh.npz';np.savez_compressed(mesh_path,**mesh)
    gained=[c for c in product['curves'] if not coupled.request_keys(poses,[c['pair']])<=required]
    report=dict(place=name,module=owner,affected=sorted(affected),setback_m=0.,cut_plan=serialize_cut_plan(network,[]),
        changed_frame_sections=[],vehicle=network.vehicle,all_requests_preserved=True,external_frames_unchanged=True,
        all_body_curves_on_native_mesh=True,proofs=proofs,published_frames=coupled.published_frames(products),
        shared_public_addition_wkb=shared_crossing.wkb.hex(),pedestrian_geometry_retained=True,
        published_surfaces=surfaces,physical_mesh_audit=audit,physical_mesh=str(mesh_path),
        physical_mesh_sha256=coupled.digest(mesh_path),original_surface_wkb=product['base'].wkb.hex(),
        original_physical_shape_wkb=network.shape(owner).wkb.hex(),
        planned_surface_wkb=product['planned'].wkb.hex(),physical_shape_wkb=product['physical'].wkb.hex(),
        previous_accepted=len(previous),accepted=len(product['curves']),connections=len(routes),unresolved=[r for p in products.values() for r in p['unresolved']],
        trials=[coupled.diagnostic_trial(c,product,network.vehicle,poses,routes) for c in gained]+product['unresolved'],
        added_area_m2=product['physical'].difference(network.shape(owner)).area,
        original_sections=[shapely.get_coordinates(p['line']) for p in network.junctions[owner]],
        planned_sections=[shapely.get_coordinates(p['line']) for p in network.junctions[owner]],
        shared_cuts_description=f'Unchanged shared cuts; {len(proofs)}/{len(routes)} routes; road-first building cuts',
        buildings=buildings,building_audit=building_audit,
        roadbed_island_cleanup=dict(closed_pockets=sum(s['pockets']['closed_pockets'] for s in seeds.values()),
            closed_area_m2=sum(s['pockets']['closed_area_m2'] for s in seeds.values())),
        physical_island_cleanup=product['physical_island_cleanup'],
        input_sha256=before['input_sha256']|{str(p.resolve().relative_to(root.resolve())):coupled.digest(p)
            for p in [cut_path,frame_path,receipt_path,args.reference,*args.curve,*args.crossing_area,*([args.prior] if args.prior else [])]},
        elapsed_ms=(time.perf_counter()-begin)*1000,
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        scope='All public-motor requests of one unchanged-cut owner; source buildings adapted, native body and neighbors checked; complete windows remain required.')
    sources=coupled.implementation_files(Path(__file__))+coupled.implementation_files(Path(__file__).with_name('steering_roadbed_candidate_probe.py'))
    sources+=coupled.implementation_files(Path(coupled.__file__))
    sources+=coupled.implementation_files(Path(construct_complete_module.__code__.co_filename))
    sources+=[Path(coupled.__file__).with_name('geos_triangulation.py').resolve(),Path(coupled.__file__).with_name('transport_network_recipes.json').resolve()]
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
