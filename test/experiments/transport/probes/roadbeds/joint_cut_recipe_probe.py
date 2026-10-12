"""Recompute overlapping cut recipes on one shared native mesh."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import transport_coupled_roadbed_experiment as coupled
from transport_cut_setbacks import CutSetbackRejected,displaced_source_section
from transport_shared_frames import frame_key

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--place',required=True)
parser.add_argument('--module',type=int,required=True)
parser.add_argument('--pair',type=int,nargs=2,required=True)
parser.add_argument('--sections',type=int,nargs='+',required=True)
parser.add_argument('--setback',type=float,required=True)
parser.add_argument('--prior',type=Path,required=True)
parser.add_argument('--repeat-junction',action='store_true')
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
root=Path('build/experiments/transport')
prior=json.loads(args.prior.read_text())
assert prior['place']==args.place
assert (prior['module']==args.module)==args.repeat_junction
assert {c['junction'] for c in prior['cut_plan']['changes']}=={prior['module']}
for name,value in prior['input_sha256'].items():assert coupled.digest(root/name)==value
for name,value in prior['implementation_sha256'].items():assert coupled.digest(Path(name))==value
assert coupled.digest(Path(prior['physical_mesh']))==prior['physical_mesh_sha256']
prior_sections={s['section'] for s in prior['cut_plan']['sections']}
assert set(prior['changed_frame_sections'])==prior_sections
requested_sections=set(args.sections)
if not args.repeat_junction:assert not requested_sections&prior_sections
original=coupled.setback_network


def joint_setback(network,module,sections,distance):
    assert module==args.module and sections==requested_sections|prior_sections
    prepared,previous_changes,previous_owners=original(network,prior['module'],prior_sections,prior['setback_m'])
    for pose in prior['published_frames']:
        if pose['section'] in prior_sections:
            prepared.frames[frame_key(pose)]={k:np.array(pose[k]) for k in ('position','heading')}
    try:
        trial,changes,affected=original(prepared,module,requested_sections,distance)
    except CutSetbackRejected:
        args.output.mkdir(parents=True,exist_ok=True)
        poses,_=prepared.poses(module,{})
        for section in requested_sections:
            pose=next(p for p in poses if p['section']==section)
            point,tangent,_=displaced_source_section(prepared,pose,distance)
            road=prepared.shape(pose['corridor'])
            normal=np.array([-tangent[1],tangent[0]])/np.linalg.norm(tangent)
            extent=np.linalg.norm(np.subtract(road.bounds[2:],road.bounds[:2]))+1
            ray=shapely.LineString([point-normal*extent,point+normal*extent])
            parts=shapely.get_parts(road.intersection(ray))
            print(json.dumps(dict(section=section,spans=[dict(kind=p.geom_type,length_m=p.length,
                  distance_m=p.distance(shapely.Point(point))) for p in parts])),flush=True)
            (args.output/f'cut-{section}-input.json').write_text(json.dumps(dict(
                core=prepared.shape(module).wkb.hex(),road=road.wkb.hex(),
                section=prepared.sections[section].wkb.hex(),point=point.tolist(),tangent=tangent.tolist()))+'\n')
        raise
    return trial,previous_changes+changes,previous_owners|affected


coupled.setback_network=joint_setback
began=time.perf_counter()
report=coupled.real_case(root,args.place,args.module,args.pair,args.setback,args.output,
                         requested_sections|prior_sections)
keys=lambda r:{(p['module'],json.dumps(p['request'])) for p in r['proofs']}
assert keys(prior)<=keys(report),'previous coupled routes lost'
sources=coupled.implementation_files(Path(__file__))
sources+=coupled.implementation_files(Path(coupled.__file__))
sources += [Path('test/experiments/transport/geos_triangulation.py').resolve(),
            Path('test/experiments/transport/transport_network_recipes.json').resolve()]
report['input_sha256'][str(args.prior.resolve().relative_to(root.resolve()))]=coupled.digest(args.prior)
report.update(elapsed_ms=(time.perf_counter()-began)*1000,
    measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
    implementation_sha256={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in sources},
    previous_coupled_requests_retained=len(keys(prior)),
    shared_cuts_description=f'Cuts moved {args.setback:g} m at {args.module} and {prior["setback_m"]:g} m at {prior["module"]}; '
                           f'{len(report["proofs"])} proved routes across {len(report["affected"])} modules',
    scope='All public-motor requests of the affected junctions and corridors, on one shared mesh; no runtime integration.',
    experimental_recipe='Replayed neighboring cut and frame recipes; all affected owners solved and triangulated together.')
(args.output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda obj:obj.tolist())+'\n')
print(json.dumps({k:report[k] for k in ('accepted','connections','affected','added_area_m2','elapsed_ms',
                                      'previous_coupled_requests_retained')}),flush=True)
