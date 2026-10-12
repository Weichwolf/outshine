from pathlib import Path
import inspect,json,sys
import numpy as np
sys.path.insert(0,'test/experiments/transport')
import transport_coupled_roadbed_experiment as coupled
from transport_cut_setbacks import setback_network
exec(Path('test/experiments/transport/probes/roadbeds/basel396_endpoints.py').read_text().split('rows=[]')[0])
source=inspect.getsource(coupled.construct_module)
source=source.replace("            assert candidate is not None,(owner,pair,[n.get('reason') for n in notes])","            if candidate is None:continue")
ns=dict(coupled.__dict__);exec(source,ns)
original=ns['construct_module']
required={r['module']:r for r in before['modules']}
required.update((r['module'],r) for r in json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text())['changed_modules'])
def construct(*args):
 product=original(*args);network,trial,owner=args[:3]
 old,_=network.poses(owner,{})
 record=required[owner] if owner in required else network.record(owner)[0]
 needed=coupled.request_keys(old,[(r['entry'],r['exit']) for r in record['turns'] if r['accepted']])
 got=coupled.request_keys(product['poses'],[c['pair'] for c in product['curves']])
 assert needed<=got,('previous routes lost',owner,needed-got)
 return product
coupled.construct_module=construct
section=poses[24]['section']
coupled.setback_network=lambda network,module,sections,distance:setback_network(network,module,{section},distance)
output=root/'roadbeds/basel396-partial-coupled-probe'
report=coupled.real_case(root,name,396,(24,20),12.,output)
report['scope']='Prototype of partial coupled solve; original accepted routes preserved; unresolved ledger not integrated'
(output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda a:a.tolist())+'\n')
print(json.dumps({k:report[k] for k in ('previous_accepted','accepted','affected','added_area_m2')}))
