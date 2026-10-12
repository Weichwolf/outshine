"""Construct rounded source bends after projecting terminal corners onto fixed lane rays."""

import argparse
import json
from pathlib import Path
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from rounded_lane_loop_probe import rounded_polyline
from steering_roadbed_candidate_probe import candidate
from turning_space_probe import provenance
from transport_cut_setback_experiment import digest,implementation_files


def terminal_lane_guide(source,entry,exit):
    points=shapely.get_coordinates(source)
    delta=np.diff(points,axis=0);lengths=np.linalg.norm(delta,axis=1)
    assert np.all(lengths>0),'source has collapsed segments'
    unit=delta/lengths[:,None]
    angles=np.arctan2(unit[:-1,0]*unit[1:,1]-unit[:-1,1]*unit[1:,0],np.sum(unit[:-1]*unit[1:],axis=1))
    points=points[np.r_[True,np.abs(angles)>1e-7,True]]
    assert len(points)>=4,'terminal lane recipe requires separate source bends'
    a=(points[1]-entry['position'])@entry['heading'];b=(exit['position']-points[-2])@exit['heading']
    assert min(a,b)>0,'source bends lie outside the terminal approach rays'
    points[1]=entry['position']+a*entry['heading'];points[-2]=exit['position']-b*exit['heading']
    points[0]=entry['position'];points[-1]=exit['position']
    return points


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--pedestrian-crossings',action='store_true')
    args=parser.parse_args();data=json.loads(args.input.read_text());args.output.mkdir(parents=True,exist_ok=True)
    floor,source,other=(shapely.from_wkb(data[k]) for k in ('surface','source_route','non_motor_reservations'))
    walk=shapely.from_wkb(data['public_pedestrian_surface']) if args.pedestrian_crossings else shapely.Polygon()
    data['reservations']=other.difference(walk).difference(floor).wkb.hex()
    entry,exit=({k:np.array(v) for k,v in data[name].items()} for name in ('entry','exit'))
    points=terminal_lane_guide(source,entry,exit);rows=[];started=time.perf_counter()
    radius=data['vehicle']['minimumRearAxleRadiusM']
    for r in (1.05*radius,1.125*radius):
        for transition in (.25,.5):
            row=dict(radius_m=r,transition_m=transition,accepted=False);rows.append(row)
            try:
                controls=rounded_polyline(points,r,transition,entry,exit)
                note=dict(curve_controls=controls.tolist());road,_=candidate(data,note)
            except (AssertionError,ValueError) as error:row['reason']=str(error);continue
            row.update(accepted=True,length_m=road['path'].length,added_area_m2=road['addition'].area)
            (args.output/f'{len(rows)-1}-candidate.json').write_text(json.dumps(note,indent=2)+'\n')
            (args.output/f'{len(rows)-1}-geometry.json').write_text(json.dumps(dict(original=floor.wkb.hex(),
                planned=road['surface'].wkb.hex(),crossing=road['surface'].intersection(walk).difference(floor).wkb.hex(),
                path=shapely.get_coordinates(road['path']).tolist()),indent=2)+'\n')
            print(json.dumps(row),flush=True)
    receipt=provenance()
    sources=[Path(__file__),Path(rounded_polyline.__code__.co_filename),*implementation_files(Path(candidate.__code__.co_filename))]
    receipt['implementation_sha256'].update({str(p.resolve().relative_to(Path.cwd())):digest(p) for p in sources})
    (args.output/'results.json').write_text(json.dumps(dict(cases=rows,input=str(args.input),input_sha256=digest(args.input),
        elapsed_ms=(time.perf_counter()-started)*1000,**receipt),indent=2)+'\n')
    print(json.dumps(dict(trials=len(rows),accepted=sum(r['accepted'] for r in rows))),flush=True)


if __name__=='__main__':main()
