"""Add a checked curve to a complete coupled product without changing prior contacts."""

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
from infrastructure_coupled_roadbed_products import apply_coupled_product
from infrastructure_fitted_roadbeds import spline_roadbed
from infrastructure_roadbed_void_recipe import close_unusable_voids
from steering_roadbed_candidate_probe import candidate


def restore(root,prior_path):
    prior=json.loads(prior_path.read_text());name=prior['place']
    before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
    frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
    network=coupled.prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
    cuts=json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text())
    coupled.restore_cut_plan(network,cuts['plan'])
    records={r['module']:r for r in before['modules']}
    records.update((r['module'],r) for r in cuts['changed_modules'])
    original=network
    network,_,_,_=apply_coupled_product(network,records,set(),root,prior_path,set())
    return prior,original,network,records


def existing_products(prior,network):
    products={}
    for row in prior['published_surfaces']:
        owner=row['module'];poses,_=network.poses(owner,{})
        routes,_=coupled.directed_module_routes(network.graph,poses,network.shape(owner),network.tree)
        pairs={next(iter(coupled.request_keys(poses,[pair]))):pair for pair in routes}
        curves=[]
        for proof in (p for p in prior['proofs'] if p['module']==owner):
            pair=pairs[tuple(map(tuple,proof['request']))]
            pieces=[dict(p,begin=np.array(p['begin']),end=np.array(p['end']),
                fit={k:np.asarray(v) for k,v in p['fit'].items()}) for p in proof['pieces']]
            curves.append(dict(pair=pair,source_route=routes[pair],existing_surface=True,
                candidate=dict(path=shapely.LineString(proof['path']),pieces=pieces,roadbed=shapely.Polygon(),
                               report=dict(reason='retained_prior_curve'))))
        base=shapely.from_wkb(row['public_surface_wkb']);core=shapely.from_wkb(row['physical_shape_wkb'])
        products[owner]=dict(base=base,core=core,planned=base,physical=core,
            quantum=row['coordinate_precision_m'],curves=curves,poses=poses,routes=routes,
            connections=len(routes),unresolved=[p for p in prior['unresolved'] if p['module']==owner])
    return products


def add_curve(root,network,products,owner,pair,note):
    product=products[owner];poses=product['poses'];route=product['routes'][pair]
    assert pair not in [tuple(c['pair']) for c in product['curves']],'request already proved'
    floor=product['base'];shape=product['core'];error=network.curbs['arc_error_m']
    receipt=json.loads((root/'inputs'/f'{network.name}-input.json').read_text())['receipt']
    buildings,_=coupled.building_reservations(receipt,floor.envelope.buffer(10))
    reserved=buildings.union(coupled.reservations(network,floor))
    support,construction=spline_roadbed(np.array(note['curve_controls']),network.vehicle,error)
    assert support is not None,construction
    base,recipe=close_unusable_voids(floor,support,reserved)
    filled=base.difference(floor)
    data=dict(surface=base.wkb.hex(),reservations=reserved.wkb.hex(),source_route=route.wkb.hex(),
        vehicle=network.vehicle,error_m=error,
        entry={k:poses[pair[0]][k] for k in ('position','heading')},
        exit={k:poses[pair[1]][k] for k in ('position','heading')})
    seed,_=candidate(data,note)
    seed['report']={k:v for k,v in note.items() if k!='curve_controls'}
    planned=seed['surface'];addition=planned.difference(floor)
    sections=shapely.STRtree(list(network.sections.values()))
    assert addition.intersection(reserved).area<=1e-8,'reservation overlap'
    assert not len(sections.query(addition,predicate='dwithin',distance=1e-7)),'changed shared cut'
    ids=network.mesh[1].query(addition,predicate='intersects')
    owners=set(map(int,network.product['face_owners'][ids]))|set(products)
    assert not any(addition.intersection(network.shape(m)).area>1e-8 for m in owners if m!=owner),'owner overlap'
    product.update(base=base,core=shape.union(filled),planned=planned,physical=shape.union(addition))
    product['curves'].append(dict(pair=pair,source_route=route,candidate=seed,existing_surface=False))
    key=next(iter(coupled.request_keys(poses,[pair])))
    product['unresolved']=[p for p in product['unresolved'] if tuple(map(tuple,p['request']))!=key]
    return recipe,addition.area


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prior',type=Path,required=True)
    parser.add_argument('--module',type=int,required=True)
    parser.add_argument('--pair',type=int,nargs=2,required=True)
    parser.add_argument('--curve',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();root=Path('build/experiments/infrastructure');begin=time.perf_counter()
    prior,original,network,records=restore(root,args.prior)
    products=existing_products(prior,network)
    recipe,addition=add_curve(root,network,products,args.module,tuple(args.pair),json.loads(args.curve.read_text()))
    precision=json.loads((root/'directed-surfaces'/f'{network.name}-flat-surfaces.json').read_text())['precision_m']
    mesh,audit,proofs,surfaces=coupled.publish_coupled_roadbeds(products,network,precision)
    old_keys={(p['module'],json.dumps(p['request'])) for p in prior['proofs']}
    assert old_keys<={(p['module'],json.dumps(p['request'])) for p in proofs}
    args.output.mkdir(parents=True,exist_ok=True);mesh_path=args.output/'mesh.npz';np.savez_compressed(mesh_path,**mesh)
    product=products[args.module];report=dict(prior)
    report.update(proofs=proofs,published_surfaces=surfaces,published_frames=coupled.published_frames(products),
        physical_mesh=str(mesh_path),physical_mesh_sha256=coupled.digest(mesh_path),physical_mesh_audit=audit,
        original_surface_wkb=product['base'].wkb.hex(),planned_surface_wkb=product['planned'].wkb.hex(),
        physical_shape_wkb=product['physical'].wkb.hex(),unresolved=[p for v in products.values() for p in v['unresolved']],
        accepted=len(product['curves']),connections=product['connections'],previous_coupled_requests_retained=len(old_keys),
        previous_accepted=prior['accepted'],void_recipe=recipe,incremental_added_area_m2=addition,
        added_area_m2=prior['added_area_m2']+addition,
        shared_cuts_description=f"Unchanged prior cuts; {len(proofs)} checked routes across {len(products)} owners",
        experimental_recipe='Extend an existing coupled mesh with explicit unusable-island construction; preserve prior curves.',
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        scope='Complete coupled mesh and all prior public-motor routes retained; no runtime integration.',
        elapsed_ms=(time.perf_counter()-begin)*1000)
    owner_poses,_=original.poses(args.module,{})
    owner_routes,_=coupled.directed_module_routes(original.graph,owner_poses,original.shape(args.module),original.tree)
    report['trials']=[coupled.diagnostic_trial(c,product,network.vehicle,owner_poses,owner_routes) for c in product['curves']]
    report['input_sha256']=dict(prior['input_sha256'])
    for path in (args.prior,args.curve):report['input_sha256'][str(path.resolve().relative_to(root.resolve()))]=coupled.digest(path)
    report['implementation_sha256']={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in coupled.implementation_files(Path(__file__))}
    report_path=args.output/'proof.json'
    report_path.write_text(json.dumps(report,indent=2,default=lambda v:v.tolist())+'\n')
    apply_coupled_product(original,records,set(),root,report_path,set())
    print(json.dumps(dict(proved=len(proofs),previous=len(old_keys),module_accepted=report['accepted'],
        closed_area_m2=recipe['closed_area_m2'],added_area_m2=addition,elapsed_ms=report['elapsed_ms'])))


if __name__=='__main__':main()
