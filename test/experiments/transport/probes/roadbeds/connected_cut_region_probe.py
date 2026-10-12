"""Rebuild neighboring cut recipes together and retain their previously proved requests."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import transport_coupled_roadbed_experiment as coupled
from transport_shared_frames import frame_key


def retained_requests(references):
    result={}
    for report in references:
        for proof in report['proofs']:
            result.setdefault(proof['module'],set()).add(tuple(map(tuple,proof['request'])))
    return result


def connected_cuts(references,original):
    recipes=[]
    for report in references:
        junctions={row['junction'] for row in report['cut_plan']['changes']}
        assert junctions=={report['module']},'reference needs its own multi-junction recipe'
        sections={row['section'] for row in report['cut_plan']['sections']}
        assert set(report['changed_frame_sections'])==sections,'reference changes undeclared outer frames'
        recipes.append((report['module'],sections,report['setback_m'],report['published_frames']))
    assert len(recipes)<=16,'local recipe budget exceeded'
    previous_sections={section for _,sections,_,_ in recipes for section in sections}

    def build(network,module,sections,distance):
        changes=[];affected=set();prepared=network
        used=set()
        for owner,cuts,setback,frames in [*recipes,(module,sections-previous_sections,distance,[])]:
            assert not used&cuts,'shared cut requested twice'
            used.update(cuts)
            prepared,moved,owners=original(prepared,owner,cuts,setback)
            changes.extend(moved);affected.update(owners)
            for pose in frames:
                if pose['section'] in cuts:
                    prepared.frames[frame_key(pose)]={k:np.array(pose[k]) for k in ('position','heading')}
        return prepared,changes,affected
    return build,previous_sections


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--place',required=True)
    parser.add_argument('--module',type=int,required=True)
    parser.add_argument('--pair',type=int,nargs=2,required=True)
    parser.add_argument('--sections',type=int,nargs='+',required=True)
    parser.add_argument('--setback',type=float,required=True)
    parser.add_argument('--reference',type=Path,action='append',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();root=Path('build/experiments/transport')
    references=[json.loads(path.read_text()) for path in args.reference]
    for report in references:
        assert report['place']==args.place
        for name,value in report['input_sha256'].items():assert coupled.digest(root/name)==value
    build,previous_sections=connected_cuts(references,coupled.setback_network)
    selected=set(args.sections);all_sections=selected|previous_sections
    assert not selected&previous_sections,'target cut already belongs to a reference'
    retained=retained_requests(references)
    begin=time.perf_counter()
    report=coupled.real_case(root,args.place,args.module,args.pair,args.setback,args.output,all_sections,
                            make_cuts=build,required_extra=retained)
    report['changed_frame_sections']=sorted(set(report['changed_frame_sections'])|all_sections)
    proved=retained_requests([report])
    assert all(requests<=proved.get(owner,set()) for owner,requests in retained.items()),'previous product routes lost'
    report['previous_product_requests_retained']=sum(map(len,retained.values()))
    report['input_sha256'].update((str(path.resolve().relative_to(root.resolve())),coupled.digest(path)) for path in args.reference)
    sources=coupled.implementation_files(Path(coupled.__file__))
    sources+=[Path(__file__).resolve(),Path('test/experiments/transport/geos_triangulation.py').resolve(),
              Path('test/experiments/transport/transport_network_recipes.json').resolve()]
    report.update(implementation_sha256={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in sources},
        elapsed_ms=(time.perf_counter()-begin)*1000,
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        shared_cuts_description=f'Connected cut recipes; {len(report["proofs"])} routes across {len(report["affected"])} owners',
        scope='Reconstructed reference cut recipes and target on one physical mesh; all previous requests re-proved. No runtime integration.')
    (args.output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda x:x.tolist())+'\n')
    print(json.dumps({k:report[k] for k in ('accepted','connections','affected','previous_product_requests_retained','added_area_m2','elapsed_ms')}))


if __name__=='__main__':main()
