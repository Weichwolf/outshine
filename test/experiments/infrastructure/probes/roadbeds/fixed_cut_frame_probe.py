"""Compare shared port angles with fixed cuts and fully checked neighboring roadbeds."""

import argparse
import json
from pathlib import Path
import resource
import subprocess
import sys
import time

import numpy as np

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import infrastructure_coupled_roadbed_experiment as coupled
from infrastructure_cut_setbacks import fork_cut_plan
from infrastructure_joint_roadbed_frames import joint_heading_trials
from infrastructure_shared_frames import frame_key


def fixed_cuts(network,module,sections,distance):
    assert distance==0 and module in network.junctions and sections
    poses,_=network.poses(module,{})
    selected=[p for p in poses if p['section'] in sections]
    assert {p['section'] for p in selected}==sections
    affected={module}|set.union(*(network.owners[frame_key(p)] for p in selected))
    return fork_cut_plan(network,module,set()),[],affected


def all_heading_trials(network,module,request,maximum_candidates=64):
    import infrastructure_joint_roadbed_frames as frames
    poses,_=network.poses(module,{})
    contacts={frame_key(p)[:3]:p for p in poses}
    ports=[contacts[key] for key in request]
    owners=set.union(*(network.owners[frame_key(p)] for p in ports))
    floors={owner:frames.motor_surface(network.shape(owner),network.graph,network.tree,network.mesh,
                    network.vehicle,network.curbs['corner_radius_m'],network.curbs['arc_error_m']) for owner in owners}
    options=[frames.heading_choices(p,network.anchors[frame_key(p)],
             [floors[k] for k in network.owners[frame_key(p)]],network.vehicle,network.curbs['arc_error_m']) for p in ports]
    choices=sorted(((a,b) for a in options[0] for b in options[1]),key=lambda pair:sum(h[1] for h in pair))
    changed={p['section'] for p in ports}
    tried=0
    for selected in choices:
        if all(np.linalg.norm(p['heading']-h[0])<1e-10 for p,h in zip(ports,selected)):continue
        if tried>=maximum_candidates:break
        trial=fork_cut_plan(network,module,set())
        for pose,(heading,_) in zip(ports,selected):
            trial.frames[frame_key(pose)]=dict(position=pose['position'].copy(),heading=heading.copy())
        tried+=1
        yield trial,changed,owners,sum(h[1] for h in selected)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path('build/experiments/infrastructure'))
    parser.add_argument('--place',required=True)
    parser.add_argument('--module',type=int,required=True)
    parser.add_argument('--pair',type=int,nargs=2,required=True)
    parser.add_argument('--all-headings',action='store_true')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();began=time.perf_counter()
    coupled.setback_network=fixed_cuts
    coupled.joint_heading_trials=all_heading_trials if args.all_headings else joint_heading_trials
    report=coupled.real_case(args.root,args.place,args.module,args.pair,0.,args.output)
    sources=set(coupled.implementation_files(Path(coupled.__file__)))|set(coupled.implementation_files(Path(__file__)))
    sources.add(Path(coupled.__file__).with_name('geos_triangulation.py').resolve())
    sources.add(Path(coupled.__file__).with_name('infrastructure_network_recipes.json').resolve())
    report.update(elapsed_ms=(time.perf_counter()-began)*1000,
        peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*(1 if sys.platform=='darwin' else 1024),
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        implementation_sha256={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in sources},
        experimental_recipe='Fixed cuts; shared headings; '+('all curve recipes' if args.all_headings else 'direct-fit angle filter'))
    report['shared_cuts_description']=f'Fixed cuts; shared headings; {len(report["proofs"])} proved routes'
    (args.output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda obj:obj.tolist())+'\n')
    print(json.dumps({k:report[k] for k in ('place','module','accepted','connections','affected','added_area_m2','elapsed_ms')}),flush=True)


if __name__=='__main__':main()
