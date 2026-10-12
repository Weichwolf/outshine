"""Reuse a proved route suffix to seed unresolved approaches to the same exit."""

import argparse
import json
from pathlib import Path
import sys
import subprocess
import time

import numpy as np
import shapely
from shapely.ops import substring

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from hermite_station_count_probe import collision_fitter
from transport_cut_setback_experiment import digest,implementation_files
from extend_coupled_product_probe import restore,existing_products
from transport_turn_roadbed_experiment import building_reservations,reservations
from transport_turn_roadbeds import roadbed_lateral_reach
from steering_roadbed_candidate_probe import candidate


def shared_suffix_guide(source,donor_source,donor_path):
    a,b=shapely.get_coordinates(source),shapely.get_coordinates(donor_source)
    count=0
    for x,y in zip(a[::-1],b[::-1]):
        if not np.allclose(x,y,atol=1e-7,rtol=0):break
        count+=1
    if count<2:return None
    anchor=a[len(a)-count]
    station=donor_path.project(shapely.Point(anchor))
    suffix=shapely.get_coordinates(substring(donor_path,station,donor_path.length))
    points=np.vstack([a[:len(a)-count],suffix])
    points=points[np.r_[True,np.linalg.norm(np.diff(points,axis=0),axis=1)>1e-10]]
    return shapely.LineString(points)


def verify():
    source=shapely.LineString([[0,0],[0,5],[10,5],[20,5]])
    donor=shapely.LineString([[-5,5],[0,5],[10,5],[20,5]])
    guide=shared_suffix_guide(source,donor,donor)
    assert guide.equals(source)
    assert shared_suffix_guide(source,shapely.reverse(donor),donor) is None
    shifted=shapely.LineString([[-5,5],[0,5.2],[10,5.2],[20,5]])
    adjusted=shared_suffix_guide(source,donor,shifted)
    xy=shapely.get_coordinates(adjusted)
    assert np.array_equal(xy[0],[0,0]) and np.array_equal(xy[-1],[20,5])
    assert np.all(np.linalg.norm(np.diff(xy,axis=0),axis=1)>0)
    entry=dict(position=np.array([0.,0.]),heading=np.array([0.,1.]))
    moved,tail,prefix=straight_entry(entry,source,2.)
    assert np.array_equal(moved['position'],[0.,2.]) and np.array_equal(prefix[0],entry['position'])
    assert np.array_equal(prefix[-1],shapely.get_coordinates(tail)[0])
    assert np.allclose(np.diff(prefix,axis=0),[[0.,.4]])
    return dict(source_prefix_retained=True,donor_direction_checked=True,endpoints_preserved=True,
                entry_straight_collinear=True)


def straight_entry(entry,guide,length):
    if length==0:return entry,guide,None
    if length<0 or not np.isfinite(length):raise ValueError('entry straight must be finite and nonnegative')
    moved=dict(entry,position=entry['position']+length*entry['heading'])
    station=guide.project(shapely.Point(moved['position']))
    tail=shapely.get_coordinates(substring(guide,station,guide.length))
    points=np.vstack([moved['position'],tail])
    points=points[np.r_[True,np.linalg.norm(np.diff(points,axis=0),axis=1)>1e-10]]
    return moved,shapely.LineString(points),np.linspace(entry['position'],moved['position'],6)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--product',type=Path,required=True)
    parser.add_argument('--module',type=int,required=True)
    parser.add_argument('--donor',type=int,nargs=2,required=True)
    parser.add_argument('--entries',type=int,nargs='+')
    parser.add_argument('--protect-cuts',action='store_true')
    parser.add_argument('--guide',type=Path)
    parser.add_argument('--entry-straight',type=float,default=0.)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    root=Path('build/experiments/transport');prior,_,network,_=restore(root,args.product)
    product=existing_products(prior,network)[args.module];poses=product['poses'];floor=product['base']
    donor=next(c for c in product['curves'] if tuple(c['pair'])==tuple(args.donor))
    receipt=json.loads((root/'inputs'/f'{network.name}-input.json').read_text())['receipt']
    buildings,_=building_reservations(receipt,floor.envelope.buffer(10))
    reserved=buildings.union(reservations(network,floor));vehicle=network.vehicle;error=network.curbs['arc_error_m']
    domain=floor.buffer(roadbed_lateral_reach(vehicle)-2*error).difference(reserved.difference(floor).buffer(2*error))
    if args.protect_cuts:
        cuts=shapely.union_all(list(network.sections.values())).intersection(domain.envelope)
        domain=domain.difference(cuts.buffer(4*error).difference(floor))
    fit=collision_fitter(33);results=[]
    for unresolved in product['unresolved']:
        pair=tuple(unresolved['pair'])
        if pair[1]!=args.donor[1]:continue
        if args.entries is not None and pair[0] not in args.entries:continue
        source=product['routes'][pair];guide=shared_suffix_guide(source,donor['source_route'],donor['candidate']['path'])
        if args.guide:guide=shapely.LineString(json.loads(args.guide.read_text())['path'])
        if guide is None:continue
        entry,guide,prefix=straight_entry(poses[pair[0]],guide,args.entry_straight)
        data=dict(surface=floor.wkb.hex(),reservations=reserved.wkb.hex(),source_route=source.wkb.hex(),
            vehicle=vehicle,error_m=error,entry={k:poses[pair[0]][k].tolist() for k in ('position','heading')},
            exit={k:poses[pair[1]][k].tolist() for k in ('position','heading')})
        (args.output/f'{pair[0]}-{pair[1]}-input.json').write_text(json.dumps(data)+'\n')
        for spacing in (8.,4.):
            begin=time.perf_counter()
            curve,note=fit(entry,poses[pair[1]],guide,domain,vehicle,error,spacing=spacing,
                maximum_nodes=17 if prefix is not None else 18,maximum_calls=1024,sample_body_edges=True,include_controls=True)
            row=dict(pair=pair,spacing_m=spacing,accepted=False,fit_accepted=curve is not None,
                residual_calls=note['residual_calls'],reason=note.get('reason'))
            if curve is not None:
                if prefix is not None:note['curve_controls']=np.concatenate([prefix[None,:],note['curve_controls']]).tolist()
                try:
                    value,_=candidate(data,note)
                    row.update(accepted=True,added_area_m2=value['addition'].area)
                except AssertionError as error_reason:row['reason']=str(error_reason)
                (args.output/f'{pair[0]}-{pair[1]}-{spacing:g}-candidate.json').write_text(json.dumps(note)+'\n')
            row['elapsed_ms']=(time.perf_counter()-begin)*1000;results.append(row);print(json.dumps(row),flush=True)
    report=dict(checks=verify(),cases=results,product=str(args.product),product_sha256=digest(args.product),
        protect_cuts=args.protect_cuts,entry_straight_m=args.entry_straight,donor=args.donor,
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in implementation_files(Path(__file__))},
        scope='Shared suffix initializes complete C2 fits; every final curve and coupled product still requires validation.')
    if args.guide:report.update(guide=str(args.guide),guide_sha256=digest(args.guide))
    (args.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':main()
