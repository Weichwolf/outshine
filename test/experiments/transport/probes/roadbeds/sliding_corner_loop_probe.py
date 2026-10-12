"""Redistribute terminal loop corners along fixed approach rays before constructing G2 turns."""

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


def lane_vertices(source,entry,exit):
    offsets=[(p['position']-np.array(xy))@np.array([-p['heading'][1],p['heading'][0]])
             for p,xy in ((entry,source.coords[0]),(exit,source.coords[-1]))]
    assert abs(offsets[0]-offsets[1])<1e-6
    points=np.array(shapely.offset_curve(source,float(np.mean(offsets)),join_style='mitre').coords)
    delta=np.diff(points,axis=0);unit=delta/np.linalg.norm(delta,axis=1)[:,None]
    angle=np.arctan2(unit[:-1,0]*unit[1:,1]-unit[:-1,1]*unit[1:,0],np.sum(unit[:-1]*unit[1:],axis=1))
    return points[np.r_[True,np.abs(angle)>1e-7,True]]


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
    original=lane_vertices(source,entry,exit);rows=[];started=time.perf_counter()
    middle=(original[2]-original[1])/np.linalg.norm(original[2]-original[1])
    options=[(a,b,c,r,L) for a in (0.,1.,2.,-1.,-2.) for b in (0.,2.,4.,6.) for c in (0.,1.,2.) for r in (4.2,4.5) for L in (.25,.5)]
    for a,b,c,r,L in options:
        points=original.copy();points[1]+=a*entry['heading'];points[-2]-=b*exit['heading']
        points[2]+=c*middle
        row=dict(first_slide_m=a,last_slide_m=b,middle_slide_m=c,radius_m=r,transition_m=L,accepted=False);rows.append(row)
        try:
            controls=rounded_polyline(points,r,L,entry,exit)
            note=dict(curve_controls=controls.tolist());road,_=candidate(data,note)
        except (AssertionError,ValueError) as error:row['reason']=str(error);continue
        row.update(accepted=True,length_m=road['path'].length,added_area_m2=road['addition'].area)
        crossing=road['surface'].intersection(walk).difference(floor)
        (args.output/f'{len(rows)-1}-candidate.json').write_text(json.dumps(note,indent=2)+'\n')
        (args.output/f'{len(rows)-1}-geometry.json').write_text(json.dumps(dict(original=floor.wkb.hex(),
            planned=road['surface'].wkb.hex(),crossing=crossing.wkb.hex(),path=shapely.get_coordinates(road['path']).tolist()),indent=2)+'\n')
        print(json.dumps(row),flush=True)
    receipt=provenance()
    sources=[Path(__file__),Path(rounded_polyline.__code__.co_filename),*implementation_files(Path(candidate.__code__.co_filename))]
    receipt['implementation_sha256'].update({str(p.resolve().relative_to(Path.cwd())):digest(p) for p in sources})
    (args.output/'results.json').write_text(json.dumps(dict(cases=rows,input=str(args.input),input_sha256=digest(args.input),
        elapsed_ms=(time.perf_counter()-started)*1000,**receipt),indent=2)+'\n')
    print(json.dumps(dict(trials=len(rows),accepted=sum(r['accepted'] for r in rows))),flush=True)


if __name__=='__main__':main()
