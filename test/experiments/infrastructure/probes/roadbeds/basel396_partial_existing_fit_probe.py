from pathlib import Path
import inspect,json,sys
import numpy as np
sys.path.insert(0,'test/experiments/infrastructure')
import infrastructure_coupled_roadbed_experiment as coupled
from infrastructure_cut_setbacks import setback_network
exec(Path('test/experiments/infrastructure/probes/roadbeds/basel396_endpoints.py').read_text().split('rows=[]')[0])
source=inspect.getsource(coupled.construct_module)
source=source.replace("            assert candidate is not None,(owner,pair,[n.get('reason') for n in notes])","            if candidate is None:continue")
source=source.replace("        curves.append(dict(pair=pair","""        check_base,check_core,check_plan,check_physical,_=compose_roadbeds(floor,shape,[*[c['candidate']['roadbed'] for c in curves],candidate['roadbed']])
        if not preserves_voids(check_base,check_plan) or not preserves_voids(check_core,check_physical):
            print(json.dumps(dict(rejected_pair=pair,reason='combined_voids')),flush=True);continue
        extra=check_plan.difference(check_base)
        if len(section_index.query(extra,predicate='dwithin',distance=1e-7)):
            print(json.dumps(dict(rejected_pair=pair,reason='shared_section')),flush=True);continue
        curves.append(dict(pair=pair""")
from infrastructure_hermite_route_fitting import fitted_route_candidates
old_existing=coupled.existing_surface_curve
def existing(entry,exit,route,surface,vehicle,error):
 candidate=old_existing(entry,exit,route,surface,vehicle,error)
 if candidate is not None:return candidate
 path,note=fitted_route_candidates(entry,exit,route,surface,vehicle,error,include_controls=True)
 if path is None:return None
 fit=dict(controls=np.array(note.pop('curve_controls')),maximum_curvature_per_m=np.array([note['curvature_bound_per_m']]))
 piece=dict(profile='G2-quintic-spline',fit=fit,begin=entry['position'],end=exit['position'])
 return dict(path=path,pieces=[piece],roadbed=coupled.shapely.Polygon(),report=note)
ns=dict(coupled.__dict__,existing_surface_curve=existing);exec(source,ns)
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
