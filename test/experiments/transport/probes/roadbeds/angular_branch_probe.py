"""Compare bounded polynomial heading branches at real shared-cut poses."""

import argparse
from functools import partial
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,'test/experiments/transport')
from transport_curvature_paths import fit_curvature_paths,sample_curvature_paths
from transport_cut_setback_experiment import prepared_network,digest
from transport_cut_setbacks import restore_cut_plan,setback_network,CutSetbackRejected
from transport_shared_frames import frame_key,heading_choices
from transport_junction_surfaces import motor_surface
import transport_cut_setbacks as cuts


def diagnose_section(core,road,section,point,tangent):
    direction=np.asarray(tangent,dtype=float)/np.linalg.norm(tangent)
    normal=np.array([-direction[1],direction[0]])
    extent=np.linalg.norm(np.subtract(road.bounds[2:],road.bounds[:2]))+1
    ray=shapely.LineString([point-normal*extent,point+normal*extent])
    parts=shapely.get_parts(road.intersection(ray))
    return dict(point=np.asarray(point).tolist(),source_point_distance_m=float(road.distance(shapely.Point(point))),
                intersection_type=road.intersection(ray).geom_type,
                spans=[dict(kind=p.geom_type,length_m=p.length,distance_m=p.distance(shapely.Point(point)))
                       for p in parts if not p.is_empty])


def branch_fitter(turns):
    return partial(fit_curvature_paths,heading_turns=turns)


def compare_poses(poses,pair,minimum_radius,setback):
    entry,exit=(poses[i] for i in pair)
    start,end=(p['position'][None,:] for p in (entry,exit))
    angles=[np.array([np.arctan2(p['heading'][1],p['heading'][0])]) for p in (entry,exit)]
    rows=[]
    for turns in (0,-1,1):
        began=time.perf_counter()
        fit=branch_fitter(turns)(start,end,*angles)
        curvature=float(fit['maximum_curvature_per_m'][0])
        valid=bool(fit['valid'][0])
        row=dict(setback_m=setback,heading_turns=turns,valid=valid,
                 radius_ok=bool(valid and curvature*minimum_radius<=1),
                 delta_rad=float(fit['delta'][0]),length_m=float(fit['length_m'][0]),
                 minimum_radius_m=1/curvature if curvature else None,
                 closure_error_m=float(fit['closure_error_m'][0]),
                 elapsed_ms=(time.perf_counter()-began)*1000)
        if row['radius_ok']:
            row['path']=sample_curvature_paths(fit,np.linspace(0,1,129))[0][0].tolist()
        rows.append(row)
    return dict(setback_m=setback,pair=pair,
                poses=[{k:p[k].tolist() if isinstance(p[k],np.ndarray) else p[k]
                        for k in ('section','edge','direction','station','position','heading')}
                       for p in (entry,exit)],branches=rows)


def check_coupled_case(args,case):
    output=args.output/f'coupled-{case["setback_m"]:g}'
    source=Path('test/experiments/transport/transport_coupled_roadbed_experiment.py')
    command=[sys.executable,'-W','error',str(source),'--root',str(args.root),'--place',args.place,
             '--module',str(args.module),'--pair',*map(str,args.pair),'--setback',str(case['setback_m']),
             '--sections',*map(str,args.sections),'--output',str(output)]
    completed=subprocess.run(command,capture_output=True,text=True)
    row=dict(setback_m=case['setback_m'],returncode=completed.returncode)
    if completed.returncode:
        row['reason']=completed.stderr.splitlines()[-1]
    else:
        proof=json.loads((output/'proof.json').read_text())
        row.update({k:proof[k] for k in ('previous_accepted','accepted','connections','added_area_m2')})
    return row


def compare_headings(network,module,poses,pair):
    ports=[poses[i] for i in pair]
    shape=network.shape(module)
    floor=motor_surface(shape,network.graph,network.tree,network.mesh,network.vehicle,
                        network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
    options=[heading_choices(p,network.anchors[frame_key(p)],[floor],network.vehicle,
                             network.curbs['arc_error_m']) for p in ports]
    if not all(options):return dict(options=list(map(len,options)),candidates=[])
    choices=[(a,b) for a in options[0] for b in options[1]]
    starts=np.repeat(ports[0]['position'][None,:],len(choices),axis=0)
    ends=np.repeat(ports[1]['position'][None,:],len(choices),axis=0)
    angles=np.array([[np.arctan2(a[0][1],a[0][0]),np.arctan2(b[0][1],b[0][0])] for a,b in choices])
    candidates=[]
    for turns in (0,-1,1):
        fit=branch_fitter(turns)(starts,ends,angles[:,0],angles[:,1])
        valid=fit['valid']&(fit['maximum_curvature_per_m']*network.vehicle['minimumRearAxleRadiusM']<=1)
        for index in np.flatnonzero(valid):
            a,b=choices[index]
            candidates.append(dict(heading_turns=turns,headings=[a[0].tolist(),b[0].tolist()],
                                   angular_cost_rad=a[1]+b[1],
                                   minimum_radius_m=1/float(fit['maximum_curvature_per_m'][index])))
    candidates.sort(key=lambda row:row['angular_cost_rad'])
    return dict(options=list(map(len,options)),pairs=len(choices),candidates=candidates)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path('build/experiments/transport'))
    parser.add_argument('--place',default='ZuerichHauptbahnhof')
    parser.add_argument('--module',type=int,default=1453)
    parser.add_argument('--pair',type=int,nargs=2,default=[3,1])
    parser.add_argument('--sections',type=int,nargs='+',default=[9515])
    parser.add_argument('--setbacks',type=float,nargs='+',default=[0.,4.,8.,12.,16.,20.])
    parser.add_argument('--coupled',action='store_true')
    parser.add_argument('--headings',action='store_true')
    parser.add_argument('--output',type=Path,default=Path('build/experiments/transport/roadbeds/angular-branches'))
    args=parser.parse_args()
    before_path=args.root/'window-1000-body'/f'{args.place}-routes.json'
    frame_path=args.root/'boundary-optimized/hermite-routes.json'
    cut_path=args.root/'cut-setbacks'/f'{args.place}-cut-setbacks.json'
    before=json.loads(before_path.read_text());frames=json.loads(frame_path.read_text())
    network=prepared_network(args.root,before,next(p['frames'] for p in frames['places'] if p['place']==args.place))
    restore_cut_plan(network,json.loads(cut_path.read_text())['plan'])
    poses,_=network.poses(args.module,{})
    request=[frame_key(poses[i])[:3] for i in args.pair]
    cases=[]
    original_cut=cuts.setback_shared_section
    failures=[]
    def captured_cut(*values):
        try:return original_cut(*values)
        except CutSetbackRejected:
            failures.append(diagnose_section(*values))
            raise
    cuts.setback_shared_section=captured_cut
    for setback in args.setbacks:
        failures.clear()
        try:
            trial=network if setback==0 else setback_network(network,args.module,set(args.sections),setback)[0]
        except CutSetbackRejected as error:
            cases.append(dict(setback_m=setback,reason=str(error),sections=failures.copy(),branches=[]))
            continue
        poses,_=trial.poses(args.module,{})
        ids={frame_key(p)[:3]:i for i,p in enumerate(poses)}
        pair=[ids[k] for k in request]
        case=compare_poses(poses,pair,network.vehicle['minimumRearAxleRadiusM'],setback)
        if args.headings:case['heading_choices']=compare_headings(trial,args.module,poses,pair)
        cases.append(case)
    result=dict(place=args.place,module=args.module,required_radius_m=network.vehicle['minimumRearAxleRadiusM'],
                cases=cases,measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
                probe_sha256=digest(Path(__file__)),
                input_sha256={str(p):digest(p) for p in (before_path,frame_path,cut_path)},
                scope='Pose closure and analytic curvature only; body, reservations, winding and mesh unproved.')
    args.output.mkdir(parents=True,exist_ok=True)
    if args.coupled:
        result['coupled']=[check_coupled_case(args,case) for case in cases
                           if any(row['radius_ok'] for row in case['branches'])]
    (args.output/'branches.json').write_text(json.dumps(result,indent=2)+'\n')
    for case in cases:
        print(json.dumps(dict(setback_m=case['setback_m'],branches=[{k:v for k,v in row.items() if k!='path'}
                                                                for row in case['branches']])),flush=True)
    if args.coupled:print(json.dumps(dict(coupled=result['coupled'])),flush=True)


if __name__=='__main__':
    main()
