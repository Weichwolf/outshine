from pathlib import Path
import json,sys
sys.path.insert(0,'test/experiments/infrastructure')
import infrastructure_coupled_roadbed_experiment as coupled
original=coupled.construct_module
products={}
def capture(*args):
 result=original(*args);products[args[2]]=result
 return result
coupled.construct_module=capture
rows=[]
for module,pair,section in ((1704,(3,5),11180),(1453,(3,1),9515)):
 products.clear()
 try:coupled.real_case(Path('build/experiments/infrastructure'),'ZuerichHauptbahnhof',module,pair,4.,Path(f'build/experiments/infrastructure/roadbeds/split-diagnostic-{module}'),[section])
 except AssertionError as error:
  row=dict(module=module,reason=str(error),unresolved=products.get(module,{}).get('unresolved',[]))
  rows.append(row)
  print(json.dumps(dict(module=module,reason=str(error),omissions=[{k:r[k] for k in ('pair','reason','details')} for r in row['unresolved']])),flush=True)
Path('build/experiments/infrastructure/roadbeds/split_failure_diagnostic.json').write_text(json.dumps(rows,indent=2)+'\n')
