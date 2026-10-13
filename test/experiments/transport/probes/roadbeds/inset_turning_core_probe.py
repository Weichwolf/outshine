"""Place bounded G2 turning cores between unchanged source-bound straight approaches."""

import argparse
import json
from pathlib import Path
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from continuous_turn_recipe import fit_turn
from coupled_turning_court_probe import approach_segment
from transport_three_arc_paths import three_arc_seeds
from transport_junction_surfaces import same_island_winding
from steering_roadbed_candidate_probe import candidate
from turning_space_probe import provenance
from transport_cut_setback_experiment import digest,implementation_files


def core_candidates(entry,exit,radius,inset,outset=None):
    if outset is None:outset=inset
    begin=dict(position=entry['position']+inset*entry['heading'],heading=entry['heading'])
    end=dict(position=exit['position']-outset*exit['heading'],heading=exit['heading'])
    for kind,seed in three_arc_seeds(begin,end,radius):
        path=shapely.LineString(np.vstack([entry['position'],np.array(seed.coords),exit['position']]))
        if path.is_simple:yield begin,end,kind,seed,path


def attach_approaches(controls,entry,exit,begin,end,inset,outset=None):
    if outset is None:outset=inset
    pieces=[]
    if inset:pieces.append(approach_segment(entry,begin)[None])
    pieces.append(controls)
    if outset:pieces.append(approach_segment(end,exit)[None])
    return np.concatenate(pieces)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--inset',type=float,nargs='+',default=[0.,4.,8.,12.])
    parser.add_argument('--outset',type=float,nargs='+')
    parser.add_argument('--radius-scale',type=float,nargs='+',default=[1.05,1.125,1.25])
    parser.add_argument('--pedestrian-crossings',action='store_true')
    parser.add_argument('--close-new-pockets',action='store_true')
    parser.add_argument('--diagnostics',action='store_true')
    parser.add_argument('--recipe',choices=('lane-flare','loop-court','return-court'),default='lane-flare')
    args=parser.parse_args()
    if not all(np.isfinite(s) and s>=0 for s in [*args.inset,*(args.outset or [])]):parser.error('approach lengths must be finite and nonnegative')
    if len(args.radius_scale)>8 or not all(np.isfinite(r) and r>=1 for r in args.radius_scale):
        parser.error('radius scales need 1–8 finite values at least one')
    data=json.loads(args.input.read_text());args.output.mkdir(parents=True,exist_ok=True)
    floor,source,other=(shapely.from_wkb(data[k]) for k in ('surface','source_route','non_motor_reservations'))
    walk=shapely.from_wkb(data['public_pedestrian_surface']) if args.pedestrian_crossings else shapely.Polygon()
    data['reservations']=other.difference(walk).difference(floor).wkb.hex()
    entry,exit=({k:np.array(v) for k,v in data[name].items()} for name in ('entry','exit'))
    rows=[];calls=0;started=time.perf_counter();radius=data['vehicle']['minimumRearAxleRadiusM']
    options=[]
    for inset in args.inset:
        for outset in args.outset if args.outset is not None else [inset]:
            for r in (radius*scale for scale in args.radius_scale):
                for begin,end,kind,seed,path in core_candidates(entry,exit,r,inset,outset):
                    if same_island_winding(path,source,floor):options.append((inset,outset,r,begin,end,kind,seed))
    for inset,outset,r,begin,end,kind,seed in options:
        if calls>=1536:break
        row=dict(inset_m=inset,outset_m=outset,radius_m=r,kind=kind,accepted=False);rows.append(row)
        controls,note=fit_turn(begin,end,seed,r,kind[0],1.,maximum_calls=min(128,1536-calls))
        calls+=note['residual_calls'];row.update(reason=note['reason'],residual_calls=note['residual_calls'])
        if controls is None:continue
        controls=attach_approaches(controls,entry,exit,begin,end,inset,outset)
        note.update(curve_controls=controls.tolist(),road_space_recipe=args.recipe,close_new_pockets=args.close_new_pockets)
        try:road,_=candidate(data,note)
        except AssertionError as error:
            row['reason']=str(error)
            if args.diagnostics:
                (args.output/f'{len(rows)-1}-rejected.json').write_text(json.dumps(note,indent=2)+'\n')
            continue
        row.update(accepted=True,reason='proved',length_m=road['path'].length,added_area_m2=road['addition'].area)
        (args.output/f'{len(rows)-1}-candidate.json').write_text(json.dumps(note,indent=2)+'\n')
        (args.output/f'{len(rows)-1}-geometry.json').write_text(json.dumps(dict(original=floor.wkb.hex(),
            planned=road['surface'].wkb.hex(),crossing=road['roadbed'].intersection(walk).difference(floor).wkb.hex(),
            path=shapely.get_coordinates(road['path']).tolist()),indent=2)+'\n')
        print(json.dumps(row),flush=True)
    receipt=provenance()
    sources=[Path(__file__),Path(approach_segment.__code__.co_filename),*implementation_files(Path(candidate.__code__.co_filename))]
    receipt['implementation_sha256'].update({str(p.resolve().relative_to(Path.cwd())):digest(p) for p in sources})
    report=dict(cases=rows,residual_calls=calls,maximum_calls=1536,input=str(args.input),
                input_sha256=digest(args.input),elapsed_ms=(time.perf_counter()-started)*1000,**receipt)
    (args.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(trials=len(rows),accepted=sum(r['accepted'] for r in rows),residual_calls=calls)),flush=True)


if __name__=='__main__':main()
