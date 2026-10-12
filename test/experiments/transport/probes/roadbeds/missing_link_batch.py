"""Try bounded shared-cut recipes for missing directed links in frozen windows."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--diagnosis',type=Path,required=True)
    parser.add_argument('--place',required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--skip',type=int,nargs='*',default=[])
    args=parser.parse_args()
    source=json.loads(args.diagnosis.read_text())
    place=next(p for p in source['places'] if p['place']==args.place)
    requests={}
    for edge in place['missing_edges']:
        if edge['reason']=='already_reachable':continue
        for route in edge['routes']:
            if route['module'] not in args.skip:requests.setdefault(route['module'],route)
    args.output.mkdir(parents=True,exist_ok=True)
    commit=subprocess.check_output(['git','rev-parse','--short=9','HEAD'],text=True).strip()
    script=Path(__file__).resolve().parents[2]/'transport_coupled_roadbed_experiment.py'
    results=[]
    for module,request in sorted(requests.items()):
        output=args.output/str(module)
        log=Path(os.environ.get('TMPDIR','/tmp'))/f'outshine-missing-{args.place}-{module}-{commit}.log'
        command=[sys.executable,'-W','error',str(script),'--place',args.place,'--module',str(module),
                 '--pair',str(request['entry']),str(request['exit']),'--output',str(output)]
        began=time.perf_counter()
        with log.open('w') as stream:
            result=subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT)
        row=dict(module=module,pair=[request['entry'],request['exit']],status=result.returncode,
                 elapsed_ms=(time.perf_counter()-began)*1000,log=str(log))
        if result.returncode==0:
            proof=json.loads((output/'proof.json').read_text())
            row.update({k:proof[k] for k in ('previous_accepted','accepted','connections','affected','added_area_m2')})
            row['proof']=str(output/'proof.json')
        else:row['failure']=log.read_text().splitlines()[-1]
        results.append(row)
        print(json.dumps(row),flush=True)
        (args.output/'results.json').write_text(json.dumps(dict(diagnosis=str(args.diagnosis),
            measured_commit=commit,place=args.place,results=results),indent=2)+'\n')


if __name__=='__main__':main()
