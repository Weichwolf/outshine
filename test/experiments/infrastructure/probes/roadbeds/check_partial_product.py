import json,sys
from pathlib import Path
sys.path.insert(0,'test/experiments/infrastructure')
from infrastructure_cut_setback_experiment import prepared_network
from infrastructure_cut_setbacks import restore_cut_plan
from infrastructure_coupled_roadbed_products import apply_coupled_product
root=Path('build/experiments/infrastructure');name='BaselBadischerBahnhof'
before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
cuts=json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text());restore_cut_plan(network,cuts['plan'])
records={r['module']:r for r in before['modules']};records.update((r['module'],r) for r in cuts['changed_modules'])
reference=next(p for p in json.loads((root/'roadbeds/windows-437a9399f/window-roadbeds.json').read_text())['places'] if p['place']==name)
required={(t['module'],t['entry'],t['exit']) for t in reference['trials'] if t['accepted']}
trial,after,remapped,note=apply_coupled_product(network,records,required,root,root/'roadbeds/partial-basel396/proof.json',set())
print(json.dumps(dict(accepted=after[396]['accepted'],connections=after[396]['connections'],required=len(required),retained=len(remapped),omissions=[(r['entry'],r['exit'],r['reason']) for r in after[396]['turns'] if not r['accepted']]),indent=2))
