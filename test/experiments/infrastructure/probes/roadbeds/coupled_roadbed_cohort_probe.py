"""Rebuild solved joint roadbeds before checking their combined published network."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
args.output.mkdir(parents=True,exist_ok=True)
root=Path('build/experiments/infrastructure/roadbeds')
scripts=Path(__file__).resolve().parents[2]
cases=[
    ('ZuerichHauptbahnhof',102,(2,4),4,()),
    ('ZuerichHauptbahnhof',1706,(4,0),4,()),
    ('ZuerichHauptbahnhof',1704,(3,5),4,(11180,)),
    ('BaselBadischerBahnhof',396,(24,20),12,(1929,)),
    ('ZuerichHauptbahnhof',1453,(3,1),12,(9515,)),
    ('BaselBadischerBahnhof',736,(3,4),4,()),
    ('BaselBadischerBahnhof',738,(1,4),4,()),
    ('BaselBadischerBahnhof',821,(0,2),4,()),
    ('ZuerichHauptbahnhof',1304,(1,2),4,()),
    ('ZuerichHauptbahnhof',1447,(0,2),4,()),
    ('ZuerichHauptbahnhof',1737,(0,3),4,()),
    ('ZuerichHauptbahnhof',1770,(8,6),4,()),
    ('ZuerichHauptbahnhof',2135,(5,3),4,()),
    ('ZuerichHauptbahnhof',715,(2,0),4,(4776,)),
    ('ZuerichHauptbahnhof',1678,(4,6),8,(10997,10998,10999)),
    ('Wien',575,(0,2),4,()),
    ('Wien',685,(1,0),4,()),
    ('Wien',810,(1,0),4,()),
    ('Wien',824,(0,2),4,()),
    ('Wien',1102,(1,3),4,()),
    ('BaselBadischerBahnhof',1209,(3,4),8,()),
    ('BaselBadischerBahnhof',940,(3,5),8,()),
    ('Wien',861,(1,0),6,()),
    ('Wien',1048,(7,4),2,()),
    ('BaselBadischerBahnhof',798,(4,1),2,()),
]
results=[]
for place,module,pair,setback,sections in cases:
    output=args.output/f'{place}-{module}'
    script=scripts/('probes/roadbeds/fixed_cut_frame_probe.py' if setback==0 else 'infrastructure_coupled_roadbed_experiment.py')
    command=[sys.executable,'-W','error',str(script),
             '--place',place,'--module',str(module),'--pair',*map(str,pair),'--output',str(output)]
    if setback!=0:command.extend(['--setback',str(setback)])
    if sections:command.extend(['--sections',*map(str,sections)])
    log=Path(tempfile.gettempdir())/f'outshine-{args.output.name}-{place}-{module}.log'
    with log.open('w') as stream:
        process=subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
    row=dict(place=place,module=module,exit_code=process.returncode,log=str(log))
    if process.returncode==0:
        report=json.loads((output/'proof.json').read_text())
        row.update({k:report[k] for k in ('accepted','connections','elapsed_ms')})
        baseline=root/'combined-502657479'/f'{place}-{module}'/'proof.json'
        if not baseline.exists() and place=='ZuerichHauptbahnhof':
            baseline=root/'zuerich-reuse-final'/str(module)/'proof.json'
        if baseline.exists():
            previous=json.loads(baseline.read_text())
            row['previous_accepted']=previous['accepted']
            row['no_count_regression']=report['accepted']>=previous['accepted']
            keys=lambda r:{(p['module'],json.dumps(p['request'])) for p in r['proofs']}
            row['previous_requests_retained']=keys(previous)<=keys(report)
    results.append(row)
    print(json.dumps(row),flush=True)
    (args.output/'cohort.json').write_text(json.dumps(results,indent=2)+'\n')
assert all(row['exit_code']==0 and row.get('no_count_regression',True) and row.get('previous_requests_retained',True) for row in results)
