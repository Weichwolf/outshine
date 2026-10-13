"""Check a local shared-port extension together with every affected owner's curves."""

import argparse
import json
from pathlib import Path
import sys
import subprocess
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import transport_coupled_roadbed_experiment as coupled
from extend_coupled_product_probe import restore,existing_products
from transport_court_partition import partition_court
from transport_shared_frames import frame_key
from transport_cut_setbacks import serialize_cut_plan
from transport_coupled_roadbed_products import apply_coupled_product
from steering_roadbed_candidate_probe import candidate


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prior',type=Path,required=True)
    parser.add_argument('--module',type=int,required=True)
    parser.add_argument('--pair',type=int,nargs=2,required=True)
    parser.add_argument('--curve',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();root=Path('build/experiments/transport');started=time.perf_counter()
    prior,original,network,records=restore(root,args.prior);products=existing_products(prior,network)
    core=args.module;product=products[core];pair=tuple(args.pair);poses=product['poses'];floor=product['base']
    receipt=json.loads((root/'inputs'/f'{network.name}-input.json').read_text())['receipt']
    region=floor.envelope.buffer(10);buildings,_=coupled.building_reservations(receipt,region)
    reserved=buildings.union(coupled.reservations(network,region)).difference(floor)
    data=dict(surface=floor.wkb.hex(),reservations=reserved.wkb.hex(),source_route=product['routes'][pair].wkb.hex(),
        vehicle=network.vehicle,error_m=network.curbs['arc_error_m'],
        entry={k:poses[pair[0]][k] for k in ('position','heading')},
        exit={k:poses[pair[1]][k] for k in ('position','heading')})
    seed,_=candidate(data,json.loads(args.curve.read_text()));seed['report']={}
    addition=seed['surface'].difference(floor);ports=[]
    for port in network.junctions[core]:
        if addition.distance(port['line'])>1e-7:continue
        pose=next(p for p in poses if p['section']==port['section'])
        ports.append(dict(section=port['section'],owner=port['corridor'],line=port['line'],
                          away=pose['heading']*(-1 if pose['incoming'] else 1)))
    assert ports,'no shared port requires expansion'
    owners=set(products)|{p['owner'] for p in ports};index=shapely.STRtree(list(network.sections.values()))
    for owner in sorted(owners-set(products)):
        row=records[owner] if owner in records else network.record(owner)[0]
        old,_=network.poses(owner,{})
        required=coupled.request_keys(old,[(t['entry'],t['exit']) for t in row['turns'] if t['accepted']])
        products[owner]=coupled.construct_module(network,network,owner,set(),owners,region,buildings,index,required,
                                                dict(contexts={},hits=0,misses=0))
    parts,lines,audit=partition_court({o:network.shape(o) for o in owners},core,addition,ports)
    for owner in owners:
        old,_=network.poses(owner,{})
        for pose in old:network.frames[frame_key(pose)]={k:pose[k] for k in ('position','heading')}
        patch=parts[owner].difference(network.shape(owner));products[owner]['physical']=parts[owner]
        products[owner]['planned']=products[owner]['planned'].union(patch).union(seed['roadbed'].intersection(parts[owner]))
    product['shared_public_context']=seed['roadbed'].difference(parts[core]).difference(product['base'])
    network.shapes.update(parts)
    for section,line in lines.items():
        network.sections[section]=line
        for entries in network.junctions.values():
            for port in entries:
                if port['section']==section:port['line']=line
    product['curves'].append(dict(pair=pair,source_route=product['routes'][pair],candidate=seed,existing_surface=False))
    key=next(iter(coupled.request_keys(poses,[pair])))
    product['unresolved']=[p for p in product['unresolved'] if tuple(map(tuple,p['request']))!=key]
    precision=json.loads((root/'directed-surfaces'/f'{network.name}-flat-surfaces.json').read_text())['precision_m']
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/"planned.json").write_text(json.dumps({str(o):{k:v[k].wkb.hex() for k in ("base","core","planned","physical")} for o,v in products.items()}))
    mesh,mesh_audit,proofs,surfaces=coupled.publish_coupled_roadbeds(products,network,precision)
    old_keys={(p['module'],tuple(map(tuple,p['request']))) for p in prior['proofs']}
    assert old_keys<={(p['module'],tuple(map(tuple,p['request']))) for p in proofs}
    args.output.mkdir(parents=True,exist_ok=True);np.savez_compressed(args.output/'mesh.npz',**mesh)
    result=dict(partition=audit,prior_requests_retained=len(old_keys),proof_count=len(proofs),owners=sorted(owners),accepted=len(product['curves']),
                connections=product['connections'],scope='Local mesh proof; serialization and world publication pending')
    changes=list(prior['cut_plan']['changes'])
    for port in ports:
        pose=next(p for p in poses if p['section']==port['section'])
        changes.append(dict(junction=core,corridor=port['owner'],section=port['section'],edge=pose['edge'],
                            station=pose['station'],recipe='expanded-fixed-plane-port'))
    mesh_path=args.output/'mesh.npz';report=dict(prior)
    shared=shapely.from_wkb(prior['shared_public_addition_wkb']) if prior.get('shared_public_addition_wkb') else shapely.Polygon()
    report.update(affected=sorted(owners),cut_plan=serialize_cut_plan(network,changes),
        changed_frame_sections=sorted(set(prior['changed_frame_sections'])|set(lines)),
        proofs=proofs,published_surfaces=surfaces,published_frames=coupled.published_frames(products),
        physical_mesh=str(mesh_path),physical_mesh_sha256=coupled.digest(mesh_path),physical_mesh_audit=mesh_audit,
        original_surface_wkb=floor.wkb.hex(),planned_surface_wkb=next(r['public_surface_wkb'] for r in surfaces if r['module']==core),
        physical_shape_wkb=shapely.union_all(list(parts.values())).wkb.hex(),
        original_physical_shape_wkb=shapely.union_all([original.shape(o) for o in owners]).wkb.hex(),
        accepted=len(product['curves']),connections=product['connections'],
        unresolved=[r for p in products.values() for r in p['unresolved']],
        trials=[coupled.diagnostic_trial(c,product,network.vehicle,poses,product['routes']) for c in product['curves']],
        shared_public_addition_wkb=shared.union(addition).wkb.hex(),incremental_added_area_m2=audit['added_area_m2'],
        added_area_m2=audit['added_area_m2'],previous_coupled_requests_retained=len(old_keys),
        shared_cuts_description=f"Expanded fixed-plane port; {len(proofs)} proved routes across {len(owners)} owners",
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        elapsed_ms=(time.perf_counter()-started)*1000)
    for path in (args.prior,args.curve):report['input_sha256'][str(path.resolve().relative_to(root.resolve()))]=coupled.digest(path)
    files=coupled.implementation_files(Path(__file__).resolve())
    report['implementation_sha256']={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in files}
    report_path=args.output/'proof.json'
    report_path.write_text(json.dumps(report,indent=2,default=lambda v:v.tolist())+'\n')
    apply_coupled_product(original,records,set(),root,report_path,set())
    result['scope']='Local mesh and product reload proved; full-window integration pending'
    (args.output/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))


if __name__=='__main__':main()
