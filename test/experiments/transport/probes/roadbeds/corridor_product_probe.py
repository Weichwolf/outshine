"""Publish a fixed-frame corridor and its changed junction as one native product."""

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
from transport_cut_setback_experiment import prepared_network
from transport_cut_setbacks import fork_cut_plan,restore_cut_plan,serialize_cut_plan,setback_network
from transport_directed_routes import directed_module_routes
from transport_shared_frames import frame_key
from transport_turn_roadbed_experiment import building_reservations


def module_view(module,product,network,trial,required,sections):
    poses,_=network.poses(module,{})
    routes,_=directed_module_routes(network.graph,poses,network.shape(module),network.tree)
    gained=[c for c in product['curves'] if not coupled.request_keys(product['poses'],[c['pair']])<=required]
    return dict(module=module,original_surface_wkb=product['base'].wkb.hex(),
        planned_surface_wkb=product['planned'].wkb.hex(),physical_shape_wkb=product['physical'].wkb.hex(),
        previous_accepted=len(required),accepted=len(product['curves']),
        connections=len(product['curves'])+len(product['unresolved']),
        trials=[coupled.diagnostic_trial(c,product,trial.vehicle,poses,routes) for c in gained]+product['unresolved'],
        added_area_m2=product['physical'].difference(trial.shape(module)).area,
        original_sections=[shapely.get_coordinates(network.sections[s]) for s in sorted(sections)],
        planned_sections=[shapely.get_coordinates(trial.sections[s]) for s in sorted(sections)])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--recipe',type=Path,required=True)
    parser.add_argument('--setback',type=float,default=0.)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();begin=time.perf_counter()
    root=Path('build/experiments/transport');recipe=json.loads(args.recipe.read_text())
    assert recipe['winner'] is not None
    for name,value in recipe['input_sha256'].items():assert coupled.digest(Path(name))==value
    for name,value in recipe['implementation_sha256'].items():assert coupled.digest(Path(name))==value
    name,module,pair=recipe['place'],recipe['module'],tuple(recipe['pair'])
    before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
    reference_path=root/'boundary-optimized/hermite-routes.json'
    reference=json.loads(reference_path.read_text())
    network=prepared_network(root,before,next(p['frames'] for p in reference['places'] if p['place']==name))
    cut_path=root/'cut-setbacks'/f'{name}-cut-setbacks.json'
    cuts=json.loads(cut_path.read_text());restore_cut_plan(network,cuts['plan'])
    records={r['module']:r for r in before['modules']};records.update((r['module'],r) for r in cuts['changed_modules'])
    poses,_=network.poses(module,{})
    contacts={frame_key(p)[:3]:p for p in poses};wanted=[]
    for saved in recipe['winner']['frames']:
        key=tuple(saved[k] for k in ('section','edge','direction'));pose=contacts[key]
        assert np.array_equal(pose['position'],saved['position']) and abs(pose['station']-saved['station'])<1e-7
        if not np.allclose(pose['heading'],saved['heading'],atol=1e-12,rtol=0):wanted.append(saved)
    assert wanted
    sections={p['section'] for p in wanted}
    affected=set.union(*(network.owners[frame_key(contacts[tuple(p[k] for k in ('section','edge','direction'))])]
                         for p in wanted))
    assert module in affected
    junction=min(affected&network.junctions.keys())
    if args.setback:
        trial,changes,affected=setback_network(network,junction,sections,args.setback)
    else:
        trial=fork_cut_plan(network,junction,set());changes=[]
        for saved in wanted:
            pose=contacts[tuple(saved[k] for k in ('section','edge','direction'))]
            trial.frames[frame_key(pose)]={k:np.array(saved[k]) for k in ('position','heading')}
    region=shapely.union_all([trial.shape(m) for m in affected]).envelope.buffer(
        coupled.roadbed_lateral_reach(trial.vehicle)+trial.curbs['arc_error_m'])
    receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
    buildings,tiles=building_reservations(receipt,region)
    index=shapely.STRtree(np.array(list(trial.sections.values()),dtype=object))
    required={}
    for owner in affected:
        record=records[owner] if owner in records else network.record(owner)[0]
        old,_=network.poses(owner,{})
        required[owner]=coupled.request_keys(old,[(t['entry'],t['exit']) for t in record['turns'] if t['accepted']])
    cache=dict(contexts={},hits=0,misses=0)
    products={owner:coupled.construct_module(network,trial,owner,sections,affected,region,buildings,index,
                                             required[owner],cache) for owner in sorted(affected)}
    print(json.dumps(dict(unresolved=[{k:row[k] for k in ('module','pair','reason','details')} for product in products.values()
                                     for row in product['unresolved']])),flush=True)
    target=next(iter(coupled.request_keys(poses,[pair])))
    assert target in coupled.request_keys(products[module]['poses'],[c['pair'] for c in products[module]['curves']]),'target corridor not gained'
    precision=json.loads((root/'directed-surfaces'/f'{name}-flat-surfaces.json').read_text())['precision_m']
    mesh,audit,proofs,surfaces=coupled.publish_coupled_roadbeds(products,trial,precision)
    args.output.mkdir(parents=True,exist_ok=True);mesh_path=args.output/'mesh.npz';np.savez_compressed(mesh_path,**mesh)
    product=products[junction];old_poses,_=network.poses(junction,{})
    original_routes,_=directed_module_routes(network.graph,old_poses,network.shape(junction),network.tree)
    previous=len(required[junction]);gained=[c for c in product['curves'] if not coupled.request_keys(product['poses'],[c['pair']])<=required[junction]]
    report=dict(place=name,module=junction,target_module=module,target_pair=pair,affected=sorted(affected),setback_m=args.setback,
        cut_plan=serialize_cut_plan(trial,changes),changed_frame_sections=sorted(sections),vehicle=trial.vehicle,
        source_tiles=tiles,all_requests_preserved=True,external_frames_unchanged=True,all_body_curves_on_native_mesh=True,
        proofs=proofs,published_frames=coupled.published_frames(products),published_surfaces=surfaces,
        physical_mesh_audit=audit,physical_mesh=str(mesh_path),physical_mesh_sha256=coupled.digest(mesh_path),
        original_surface_wkb=product['base'].wkb.hex(),planned_surface_wkb=product['planned'].wkb.hex(),
        physical_shape_wkb=product['physical'].wkb.hex(),previous_accepted=previous,accepted=len(product['curves']),
        connections=len(product['curves'])+len(product['unresolved']),
        unresolved=[row for p in products.values() for row in p['unresolved']],
        trials=[coupled.diagnostic_trial(c,product,trial.vehicle,old_poses,original_routes) for c in gained]+product['unresolved'],
        added_area_m2=sum(p['physical'].difference(trial.shape(owner)).area for owner,p in products.items()),
        original_sections=[shapely.get_coordinates(network.sections[s]) for s in sorted(sections)],
        planned_sections=[shapely.get_coordinates(trial.sections[s]) for s in sorted(sections)],
        shared_cuts_description=f'Cut moved {args.setback:g} m; corridor {module} and junction {junction}; {len(proofs)} proved routes',
        input_sha256=before['input_sha256']|{str(cut_path.relative_to(root)):coupled.digest(cut_path),
                       str(reference_path.relative_to(root)):coupled.digest(reference_path),
                       str(args.recipe.resolve().relative_to(root.resolve())):coupled.digest(args.recipe)},
        elapsed_ms=(time.perf_counter()-begin)*1000,
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        scope='All public-motor requests of the changed corridor and junction, on one shared native mesh; full windows remain required.')
    sources=coupled.implementation_files(Path(__file__))
    sources+=[Path(coupled.__file__).with_name('geos_triangulation.py').resolve(),
              Path(coupled.__file__).with_name('transport_network_recipes.json').resolve()]
    report['implementation_sha256']={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in sources}
    report['target_view']=module_view(module,products[module],network,trial,required[module],sections)
    (args.output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda value:value.tolist())+'\n')
    print(json.dumps({k:report[k] for k in ('affected','accepted','connections','added_area_m2','elapsed_ms')}),flush=True)


if __name__=='__main__':main()
