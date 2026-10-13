"""Compare bounded turning-space recipes before integrating shared junction ownership."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from transport_arc_line_paths import arc_line_arc_seeds
from transport_three_arc_paths import three_arc_seeds
from transport_fitted_roadbeds import spline_roadbed
from transport_junction_surfaces import same_island_winding
from transport_spline_routes import checked_spline_path
from transport_turn_roadbed_experiment import mesh_coverage
from transport_turn_roadbeds import preserves_voids
from hermite_station_count_probe import collision_fitter
from continuous_turn_recipe import fit_turn
from transport_cut_setback_experiment import digest,implementation_files


def seeds(entry,exit,source,floor,region,vehicle,families=('CSC','CCC'),limit=3):
    choices=[]
    for scale in (1.05,1.125,1.25):
        radius=vehicle['minimumRearAxleRadiusM']*scale
        for family,method in (('CSC',arc_line_arc_seeds),('CCC',three_arc_seeds)):
            if family not in families:continue
            for kind,path in method(entry,exit,radius):
                if not path.is_simple or not region.covers(path) or not same_island_winding(path,source,floor):continue
                outside=path.buffer(vehicle['widthM']*.5).difference(floor).area
                choices.append((outside,path.length,family,kind,radius,path))
    return sorted(choices,key=lambda c:c[:2])[:limit]


def provenance():
    code=Path(__file__).resolve().parents[2]
    sources={Path(__file__).resolve(),Path(fit_turn.__code__.co_filename),Path(collision_fitter.__code__.co_filename)}
    for name in ('transport_fitted_roadbeds','transport_turn_roadbed_experiment','transport_three_arc_paths',
                 'transport_arc_line_paths','transport_cut_setback_experiment'):
        sources.update(implementation_files(code/(name+'.py')))
    return dict(measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
                implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sorted(sources)})


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--spacing',type=float,default=4.)
    parser.add_argument('--seed-limit',type=int,choices=range(1,25),default=3)
    parser.add_argument('--method',choices=('hermite','continuous'),default='hermite')
    parser.add_argument('--transition-length',type=float,default=1.)
    parser.add_argument('--request-building-clearance',action='store_true')
    parser.add_argument('--surface',choices=('strip','court'),default='strip')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    if not np.isfinite(args.transition_length) or args.transition_length<=0:parser.error('transition length must be finite and positive')
    data=json.loads(args.input.read_text());vehicle,error=data['vehicle'],data['error_m']
    entry,exit=({k:np.array(v) for k,v in data[p].items()} for p in ('entry','exit'))
    floor,reserved,source=(shapely.from_wkb(data[k]) for k in ('surface','reservations','source_route'))
    buildings=shapely.from_wkb(data['buildings']) if 'buildings' in data else None
    if args.request_building_clearance:
        if buildings is None or 'non_motor_reservations' not in data:parser.error('separate building/non-motor reservations required')
        reserved=shapely.from_wkb(data['non_motor_reservations']).difference(floor)
    reach=2*vehicle['minimumRearAxleRadiusM']+max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])+vehicle['widthM']*.5
    region=source.envelope.buffer(reach)
    domain=region.difference(reserved.difference(floor).buffer(2*error))
    options=seeds(entry,exit,source,floor,region,vehicle,limit=args.seed_limit);fit=collision_fitter(33);calls=0;rows=[]
    for index,(_,_,family,kind,radius,path) in enumerate(options):
        begin=time.perf_counter();budget=(1536-calls)//(len(options)-index)
        if args.method=='continuous':
            sign=kind if family=='CSC' else kind[0]
            controls,note=fit_turn(entry,exit,path,radius,sign,args.transition_length,maximum_calls=min(128,budget))
            curve=None
            if controls is not None:
                curve,proof=checked_spline_path(controls,domain,vehicle,error)
                note.update(proof,curve_controls=controls.tolist())
        else:
            curve,note=fit(entry,exit,path,domain,vehicle,error,spacing=args.spacing,maximum_calls=budget,
                sample_body_edges=True,include_controls=True)
        calls+=note['residual_calls'];row=dict(family=family,kind=kind,radius_m=radius,
            method=args.method,transition_length_m=args.transition_length,residual_calls=note['residual_calls'],accepted=False,reason=note.get('reason'));rows.append(row)
        if curve is not None:
            controls=np.array(note['curve_controls']);support,construction=spline_roadbed(controls,vehicle,error)
            if support is None:row['reason']=construction['reason']
            else:
                if args.surface=='court':
                    court=shapely.convex_hull(support)
                    construction.update(recipe='convex-turning-court',court_extra_m2=court.difference(support).area)
                    support=court
                support=support.intersection(region).difference(reserved);planned=floor.union(support)
                if not preserves_voids(floor,planned):row['reason']='changed_void_topology'
                elif not same_island_winding(curve,source,floor):row['reason']='changed_source_winding'
                else:
                    checked,proof=checked_spline_path(controls,planned,vehicle,error)
                    if checked is None:row['reason']=proof['reason']
                    else:
                        piece=dict(profile='G2-quintic-spline',fit=dict(controls=controls,
                            maximum_curvature_per_m=np.array([proof['curvature_bound_per_m']])),
                            begin=entry['position'],end=exit['position'])
                        audit,actual=mesh_coverage(planned,vehicle,[piece],error)
                        row.update(accepted=True,added_area_m2=actual.difference(floor).area,length_m=checked.length,
                            construction=construction,mesh=audit)
                        if args.request_building_clearance:
                            row.update(building_resolution_pending=True,building_clearance_m2=actual.intersection(buildings).area)
                        (args.output/f'{index}-candidate.json').write_text(json.dumps(note)+'\n')
                        (args.output/f'{index}-geometry.json').write_text(json.dumps(dict(original=floor.wkb.hex(),
                            planned=actual.wkb.hex(),path=shapely.get_coordinates(checked).tolist(),source=data['source_route']))+'\n')
                        if args.request_building_clearance:
                            (args.output/f'{index}-clearance-request.json').write_text(json.dumps(dict(
                                transport_occupied_area_wkb=actual.wkb.hex(),building_source_wkb=buildings.wkb.hex(),
                                conflict_wkb=actual.intersection(buildings).wkb.hex(),building_resolution_pending=True))+'\n')
        row['elapsed_ms']=(time.perf_counter()-begin)*1000;print(json.dumps(row),flush=True)
    (args.output/'results.json').write_text(json.dumps(dict(cases=rows,residual_calls=calls,maximum_calls=1536,
        reach_m=reach,spacing_m=args.spacing,seed_limit=args.seed_limit,input=str(args.input),input_sha256=digest(args.input),
        **provenance(),
        scope='Local turning-space recipe with unchanged vehicle and body tests; shared cuts and neighbors remain unverified.'),indent=2)+'\n')


if __name__=='__main__':main()
