"""Construct G2 corners directly on a lane-offset source loop before body validation."""

import argparse
import json
from pathlib import Path
import sys
import subprocess

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from continuous_turn_recipe import advance
from transport_hermite_route_fitting import hermite_controls
from coupled_turning_court_probe import approach_segment
from transport_spline_routes import checked_spline_path,split_controls,curve_bounds
from transport_junction_surfaces import same_island_winding
from transport_fitted_roadbeds import spline_roadbed
from transport_turn_roadbed_experiment import mesh_coverage
from transport_cut_setback_experiment import digest,implementation_files


def turn_segments(angle,radius,transition):
    sign=np.sign(angle);curvature=sign/radius
    plateau=radius*abs(angle)-transition
    if plateau<0:raise ValueError('transition exceeds corner sweep')
    return [(0.,curvature,transition),(curvature,curvature,plateau),(curvature,0.,transition)]


def corner_controls(position,heading,segments):
    pieces=[]
    for start,end,length in segments:
        count=max(1,int(np.ceil(length)));step=length/count
        for i in range(count):
            a=start+(end-start)*i/count;b=start+(end-start)*(i+1)/count
            target,angle=advance(position,heading,a,b,step)
            vectors=np.array([[np.cos(heading),np.sin(heading)],[np.cos(angle),np.sin(angle)]])
            acceleration=vectors[:,::-1]*np.array([-1.,1.])*np.array([a,b])[:,None]*step**2
            pieces.append(hermite_controls(np.c_[np.array([position,target]),vectors*step,acceleration])[0])
            position,heading=target,angle
    return pieces


def rounded_polyline(points,radius,transition,entry=None,exit=None):
    delta=np.diff(points,axis=0);unit=delta/np.linalg.norm(delta,axis=1)[:,None]
    angles=np.arctan2(unit[:-1,0]*unit[1:,1]-unit[:-1,1]*unit[1:,0],np.sum(unit[:-1]*unit[1:],axis=1))
    points=points[np.r_[True,np.abs(angles)>1e-7,True]]
    delta=np.diff(points,axis=0);lengths=np.linalg.norm(delta,axis=1);unit=delta/lengths[:,None]
    angles=np.arctan2(unit[:-1,0]*unit[1:,1]-unit[:-1,1]*unit[1:,0],np.sum(unit[:-1]*unit[1:],axis=1))
    trims=[];segments=[]
    for angle in angles:
        recipe=turn_segments(angle,radius,min(transition,radius*abs(angle)*.5));p=np.zeros(2);heading=0.
        for a,b,length in recipe:p,heading=advance(p,heading,a,b,length)
        direction=np.array([1+np.cos(angle),np.sin(angle)])
        trims.append(float(p@direction/(direction@direction)));segments.append(recipe)
    trims=np.array(trims);occupied=np.r_[0.,trims]+np.r_[trims,0.]
    assert np.all(occupied<lengths),'corner trims exceed source segment'
    controls=[];previous=dict(position=points[0],heading=unit[0]) if entry is None else entry
    for i,trim in enumerate(trims):
        begin=dict(position=points[i+1]-unit[i]*trim,heading=unit[i])
        controls.append(approach_segment(previous,begin))
        controls.extend(corner_controls(begin['position'],np.arctan2(*begin['heading'][::-1]),segments[i]))
        previous=dict(position=points[i+1]+unit[i+1]*trim,heading=unit[i+1])
    controls.append(approach_segment(previous,dict(position=points[-1],heading=unit[-1]) if exit is None else exit))
    return np.array(controls)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--lane-expansion',type=float,nargs='+',default=[0,.5,1,1.5,2])
    parser.add_argument('--pedestrian-crossings',action='store_true')
    args=parser.parse_args();data=json.loads(args.input.read_text());args.output.mkdir(parents=True,exist_ok=True)
    floor,source,other=(shapely.from_wkb(data[k]) for k in ('surface','source_route','non_motor_reservations'))
    pedestrians=shapely.from_wkb(data['public_pedestrian_surface']) if args.pedestrian_crossings else shapely.Polygon()
    hard=other.difference(pedestrians)
    entry,exit=({k:np.array(v) for k,v in data[name].items()} for name in ('entry','exit'))
    offsets=[float((p['position']-xy)@np.array([-p['heading'][1],p['heading'][0]]))
             for p,xy in ((entry,np.array(source.coords[0])),(exit,np.array(source.coords[-1])))]
    assert abs(offsets[0]-offsets[1])<1e-6,'different endpoint lanes need a taper'
    vectors=np.diff(np.array(source.coords),axis=0);headings=np.unwrap(np.arctan2(vectors[:,1],vectors[:,0]))
    orientation=np.sign(headings[-1]-headings[0])
    rows=[]
    options=[(expansion,radius,transition) for expansion in args.lane_expansion for radius in (4.2,4.5,5.) for transition in (.25,.5,1.)]
    for expansion,radius,transition in options:
        guide=shapely.offset_curve(source,np.mean(offsets)-orientation*expansion,join_style='mitre')
        points=np.array(guide.coords)
        row=dict(lane_expansion_m=expansion,radius_m=radius,transition_m=transition,accepted=False);rows.append(row)
        try:controls=rounded_polyline(points,radius,transition,entry,exit)
        except (ValueError,AssertionError) as error:row['reason']=str(error);continue
        for _ in range(3):
            bad=curve_bounds(controls)[2]*data['vehicle']['minimumRearAxleRadiusM']>1
            if not np.any(bad):break
            controls=np.concatenate([split_controls(c[None]) if split else c[None] for c,split in zip(controls,bad)])
        support,audit=spline_roadbed(controls,data['vehicle'],data['error_m'])
        if support is None:row['reason']=audit['reason'];continue
        crossing=support.intersection(pedestrians).difference(floor)
        planned=floor.union(support.difference(hard.difference(floor)))
        path,note=checked_spline_path(controls,planned,data['vehicle'],data['error_m'])
        row['reason']=note.get('reason')
        if path is None or not same_island_winding(path,source,floor):continue
        piece=dict(profile='G2-quintic-spline',fit=dict(controls=controls,
            maximum_curvature_per_m=np.array([note['curvature_bound_per_m']])),begin=entry['position'],end=exit['position'])
        mesh_coverage(planned,data['vehicle'],[piece],data['error_m'])
        row.update(accepted=True,length_m=path.length,added_area_m2=planned.difference(floor).area,
                   pedestrian_crossing_area_m2=crossing.area)
        (args.output/f'{len(rows)-1}-candidate.json').write_text(json.dumps(dict(note,curve_controls=controls.tolist()),indent=2)+'\n')
        (args.output/f'{len(rows)-1}-geometry.json').write_text(json.dumps(dict(original=floor.wkb.hex(),planned=planned.wkb.hex(),
            crossing=crossing.wkb.hex(),path=shapely.get_coordinates(path).tolist()),indent=2)+'\n')
        print(json.dumps(row),flush=True)
    (args.output/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
    sources=implementation_files(Path(__file__).resolve().parents[2]/'transport_turn_roadbed_experiment.py')
    sources.extend([Path(__file__).resolve(),Path(advance.__code__.co_filename),Path(approach_segment.__code__.co_filename)])
    (args.output/'receipt.json').write_text(json.dumps(dict(input=str(args.input),input_sha256=digest(args.input),
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources}),indent=2)+'\n')
    print(json.dumps(dict(trials=len(rows),accepted=sum(r['accepted'] for r in rows))),flush=True)


if __name__=='__main__':main()
