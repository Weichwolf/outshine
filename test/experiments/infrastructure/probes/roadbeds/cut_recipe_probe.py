"""Compare one-sided and joint cut recipes with the same source-route guarantees."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from infrastructure_cut_setback_experiment import prepared_network
from infrastructure_cut_setbacks import restore_cut_plan


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--place',required=True)
    parser.add_argument('--module',type=int,required=True)
    parser.add_argument('--pair',type=int,nargs=2,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    root=Path('build/experiments/infrastructure')
    before=json.loads((root/'window-1000-body'/f'{args.place}-routes.json').read_text())
    frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
    network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==args.place))
    cuts=json.loads((root/'cut-setbacks'/f'{args.place}-cut-setbacks.json').read_text())
    restore_cut_plan(network,cuts['plan'])
    poses,_=network.poses(args.module,{})
    a,b=(poses[i]['section'] for i in args.pair)
    recipes=list(dict.fromkeys([(a,),(b,),tuple(sorted({a,b}))]))
    radius=network.vehicle['minimumRearAxleRadiusM']
    commit=subprocess.check_output(['git','rev-parse','--short=9','HEAD'],text=True).strip()
    script=Path(__file__).resolve().parents[2]/'infrastructure_coupled_roadbed_experiment.py'
    args.output.mkdir(parents=True,exist_ok=True);rows=[]
    for distance in (radius,2*radius,3*radius):
        for sections in recipes:
            name=f'{distance:g}-'+ '-'.join(map(str,sections))
            output=args.output/name
            log=Path(os.environ.get('TMPDIR','/tmp'))/f'outshine-cut-{args.place}-{args.module}-{name}-{commit}.log'
            command=[sys.executable,'-W','error',str(script),'--place',args.place,'--module',str(args.module),
                     '--pair',*map(str,args.pair),'--setback',str(distance),'--sections',*map(str,sections),'--output',str(output)]
            began=time.perf_counter()
            with log.open('w') as stream:result=subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT)
            row=dict(distance_m=distance,sections=sections,status=result.returncode,
                     elapsed_ms=(time.perf_counter()-began)*1000,log=str(log))
            if result.returncode==0:
                proof=json.loads((output/'proof.json').read_text())
                row.update({k:proof[k] for k in ('accepted','connections','affected','added_area_m2')})
                row['proof']=str(output/'proof.json')
            else:row['failure']=log.read_text().splitlines()[-1]
            rows.append(row);print(json.dumps(row),flush=True)
            (args.output/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
            if result.returncode==0:return


if __name__=='__main__':main()
