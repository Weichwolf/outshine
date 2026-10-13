"""Reconstruct stored cut stations and shared frames, then re-prove every prior request."""

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
from transport_shared_road_space import construct_shared_space


def replay_cuts(recipe,original):
    def build(network,module,sections,distance):
        assert module==recipe['module'] and sections==set(recipe['changed_frame_sections'])
        prepared=network;changes=[];affected=set()
        for row in recipe['cut_plan']['changes']:
            owner,section=row['junction'],row['section']
            poses,_=prepared.poses(owner,{})
            pose=min((p for p in poses if p['section']==section),key=lambda p:(p['edge'],p['station'],p['direction']))
            assert pose['edge']==row['edge'] and pose['corridor']==row['corridor'],'cut source identity changed'
            shift=(row['station']-pose['station'])*pose['direction']*(-1 if pose['incoming'] else 1)
            assert shift>0,'stored cut does not advance outward'
            prepared,moved,owners=original(prepared,owner,{section},shift)
            changes.extend(moved);affected.update(owners)
        for owner in {recipe['module']}|{row['junction'] for row in recipe['cut_plan']['changes']}:
            poses,_=prepared.poses(owner,{})
            for pose in poses:
                if pose['section'] in sections:affected.update(prepared.owners[frame_key(pose)])
        assert affected==set(recipe['affected']),'replayed cut owner scope changed'
        frames={}
        for owner in sorted(affected):
            poses,_=prepared.poses(owner,{})
            for pose in poses:frames[frame_key(pose)[:3]]=pose
        for saved in recipe['published_frames']:
            if saved['section'] not in sections:continue
            key=saved['section'],saved['edge'],saved['direction'];pose=frames[key]
            assert abs(saved['station']-pose['station'])<1e-7,'stored frame source station changed'
            prepared.frames[frame_key(pose)]={k:np.array(saved[k]) for k in ('position','heading')}
        return prepared,changes,affected
    return build


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--recipe',type=Path,required=True)
    parser.add_argument('--pair',type=int,nargs=2,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();root=Path('build/experiments/transport');recipe=json.loads(args.recipe.read_text())
    for name,value in recipe['input_sha256'].items():
        if not name.startswith('roadbeds/'):
            assert coupled.digest(root/name)==value,'stored recipe source inputs changed'
    retained={}
    for proof in recipe['proofs']:
        retained.setdefault(proof['module'],set()).add(tuple(map(tuple,proof['request'])))
    sections=set(recipe['changed_frame_sections']);started=time.perf_counter()
    shared_recipe=recipe.get('construction_recipe')=='shared-road-space' or 'Missing routes retried once on constructed public space' in recipe.get('experimental_recipe','')
    if shared_recipe:
        before=json.loads((root/'window-1000-body'/f'{recipe["place"]}-routes.json').read_text())
        cuts=json.loads((root/'cut-setbacks'/f'{recipe["place"]}-cut-setbacks.json').read_text())
        records={r['module']:r for r in before['modules']};records.update((r['module'],r) for r in cuts['changed_modules'])
        original=coupled.construct_module
        def shared(network,trial,owner,sections,affected,region,buildings,index,required,cache=None):
            record=records[owner] if owner in records else network.record(owner)[0]
            poses,_=network.poses(owner,{})
            baseline=coupled.request_keys(poses,[(t['entry'],t['exit']) for t in record['turns'] if t['accepted']])
            return construct_shared_space(original,network,trial,owner,sections,affected,region,buildings,index,
                                          required,cache,baseline_required=baseline)
        coupled.construct_module=shared
    report=coupled.real_case(root,recipe['place'],recipe['module'],args.pair,recipe['setback_m'],args.output,
        sections,make_cuts=replay_cuts(recipe,coupled.setback_network),required_extra=retained)
    after={(p['module'],tuple(map(tuple,p['request']))) for p in report['proofs']}
    assert all((owner,request) in after for owner,requests in retained.items() for request in requests),'prior request lost'
    sources=coupled.implementation_files(Path(coupled.__file__))+[Path(__file__).resolve(),
        Path('test/experiments/transport/geos_triangulation.py').resolve(),
        Path('test/experiments/transport/transport_network_recipes.json').resolve()]
    report.update(measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        elapsed_ms=(time.perf_counter()-started)*1000,prior_requests_retained=sum(map(len,retained.values())),
        construction_recipe='shared-road-space' if shared_recipe else 'cut-layout',
        implementation_sha256={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in sources},
        shared_cuts_description='Reconstructed source cut stations and declared shared frames; all prior routes re-proved')
    report['input_sha256'][str(args.recipe.resolve().relative_to(root.resolve()))]=coupled.digest(args.recipe)
    (args.output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda obj:obj.tolist())+'\n')
    print(json.dumps({k:report[k] for k in ('module','accepted','connections','affected','prior_requests_retained','elapsed_ms')}))


if __name__=='__main__':main()
