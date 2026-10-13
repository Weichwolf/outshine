"""Test bounded source-guided shortcuts without changing the original route contract."""

import argparse
import json
from pathlib import Path
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from terminal_lane_alignment_probe import terminal_lane_guide
from rounded_lane_loop_probe import rounded_polyline
from steering_roadbed_candidate_probe import candidate
from turning_space_probe import provenance
from transport_cut_setback_experiment import digest,implementation_files


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--maximum-trials',type=int,default=64)
    parser.add_argument('--pedestrian-crossings',action='store_true')
    parser.add_argument('--close-new-pockets',action='store_true')
    args=parser.parse_args()
    if not 1<=args.maximum_trials<=256:parser.error('trial budget must be between 1 and 256')
    data=json.loads(args.input.read_text());args.output.mkdir(parents=True,exist_ok=True)
    floor,source,other=(shapely.from_wkb(data[k]) for k in ('surface','source_route','non_motor_reservations'))
    walk=shapely.from_wkb(data['public_pedestrian_surface']) if args.pedestrian_crossings else shapely.Polygon()
    data['reservations']=other.difference(walk).difference(floor).wkb.hex()
    points=shapely.get_coordinates(source)
    if len(points)>66:parser.error('source exceeds the 66-point recipe budget')
    entry,exit=({k:np.array(v) for k,v in data[n].items()} for n in ('entry','exit'))
    rows=[];started=time.perf_counter();budget_exhausted=False
    for begin in range(1,len(points)-1):
        for end in range(begin+1,len(points)):
            guide=shapely.LineString(np.vstack([points[:begin],points[end:]]))
            for scale in (1.05,1.125):
                if len(rows)>=args.maximum_trials:budget_exhausted=True;break
                radius=data['vehicle']['minimumRearAxleRadiusM']*scale
                row=dict(removed=[begin,end],radius_m=radius,accepted=False);rows.append(row)
                try:
                    aligned=terminal_lane_guide(guide,entry,exit)
                    controls=rounded_polyline(aligned,radius,.25,entry,exit)
                    note=dict(curve_controls=controls.tolist(),close_new_pockets=args.close_new_pockets)
                    road,_=candidate(data,note)
                except (AssertionError,ValueError) as error:row['reason']=str(error);continue
                row.update(accepted=True,length_m=road['path'].length,added_area_m2=road['addition'].area)
                index=len(rows)-1
                (args.output/f'{index}-candidate.json').write_text(json.dumps(note)+'\n')
                (args.output/f'{index}-geometry.json').write_text(json.dumps(dict(original=floor.wkb.hex(),
                    planned=road['surface'].wkb.hex(),path=shapely.get_coordinates(road['path']).tolist(),
                    crossing=road['roadbed'].intersection(walk).difference(floor).wkb.hex()))+'\n')
            if budget_exhausted:break
        if budget_exhausted:break
    receipt=provenance()
    for source_file in implementation_files(Path(__file__).resolve()):
        receipt['implementation_sha256'][str(source_file.relative_to(Path.cwd()))]=digest(source_file)
    report=dict(cases=rows,maximum_trials=args.maximum_trials,budget_exhausted=budget_exhausted,
                input=str(args.input),input_sha256=digest(args.input),elapsed_ms=(time.perf_counter()-started)*1000,**receipt)
    (args.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(trials=len(rows),accepted=sum(r['accepted'] for r in rows),budget_exhausted=budget_exhausted)))


if __name__=='__main__':main()
