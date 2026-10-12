import json,subprocess,sys,time,os
from pathlib import Path
root=Path('build/experiments/transport/roadbeds');rows=[]
for module,pair,section in ((1704,[3,5],11180),(1453,[3,1],9515)):
 for distance in (4,8,12):
  output=root/f'split-{module}-entry-{distance}'
  log=Path(os.environ.get('TMPDIR','/tmp'))/f'outshine-split-{module}-{distance}-7d48de435.log'
  command=[sys.executable,'-W','error','test/experiments/transport/transport_coupled_roadbed_experiment.py','--place','ZuerichHauptbahnhof','--module',str(module),'--pair',*map(str,pair),'--setback',str(distance),'--sections',str(section),'--output',str(output)]
  began=time.perf_counter()
  with log.open('w') as stream:result=subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT)
  row=dict(module=module,pair=pair,distance_m=distance,status=result.returncode,elapsed_s=time.perf_counter()-began,log=str(log))
  if result.returncode==0:
   report=json.loads((output/'proof.json').read_text())
   row.update(accepted=report['accepted'],connections=report['connections'],area_m2=report['added_area_m2'])
  else:row['failure']=log.read_text().splitlines()[-1]
  rows.append(row);print(json.dumps(row),flush=True)
(root/'network_split_setback_probe.json').write_text(json.dumps(rows,indent=2)+'\n')
