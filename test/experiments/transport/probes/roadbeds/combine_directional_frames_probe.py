"""Re-prove both request sets using explicitly selected directional source frames."""

import argparse
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import transport_coupled_roadbed_experiment as coupled
from replay_cut_product_probe import replay_cuts


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base',type=Path,required=True)
    parser.add_argument('--alternate',type=Path,required=True)
    parser.add_argument('--frame',type=int,nargs=3,action='append',required=True)
    parser.add_argument('--pair',type=int,nargs=2,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();root=Path('build/experiments/transport')
    base,alternate=(json.loads(p.read_text()) for p in (args.base,args.alternate))
    for report in (base,alternate):
        for name,value in report['implementation_sha256'].items():assert coupled.digest(Path(name))==value
        for name,value in report['input_sha256'].items():assert coupled.digest(root/name)==value
    for field in ('place','module','cut_plan','affected','changed_frame_sections'):
        assert base[field]==alternate[field],f'incompatible {field}'
    key=lambda row:tuple(row[k] for k in ('section','edge','direction'))
    other={key(row):row for row in alternate['published_frames']}
    selected=set(map(tuple,args.frame));assert selected<=other.keys()
    frames=[]
    for row in base['published_frames']:
        replacement=other[key(row)] if key(row) in selected else row
        assert abs(row['station']-replacement['station'])<1e-7,'frame moves source station'
        frames.append(replacement)
    recipe=dict(base,published_frames=frames);required={}
    for source in (base,alternate):
        for proof in source['proofs']:
            required.setdefault(proof['module'],set()).add(tuple(map(tuple,proof['request'])))
    report=coupled.real_case(root,base['place'],base['module'],tuple(args.pair),base['setback_m'],args.output,
        set(base['changed_frame_sections']),make_cuts=replay_cuts(recipe,coupled.setback_network),required_extra=required)
    proved={(p['module'],tuple(map(tuple,p['request']))) for p in report['proofs']}
    assert all((owner,request) in proved for owner,requests in required.items() for request in requests)
    sources=coupled.implementation_files(Path(__file__).resolve())
    report.update(measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        implementation_sha256={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in sources},
        prior_requests_retained=True,selected_directional_frames=sorted(selected))
    for path in (args.base,args.alternate):report['input_sha256'][str(path.resolve().relative_to(root.resolve()))]=coupled.digest(path)
    (args.output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda x:x.tolist())+'\n')
    print(json.dumps({k:report[k] for k in ('accepted','connections','added_area_m2','prior_requests_retained')}))


if __name__=='__main__':main()
