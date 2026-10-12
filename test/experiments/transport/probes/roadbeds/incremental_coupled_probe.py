from pathlib import Path
import inspect,json,sys
import numpy as np
sys.path.insert(0,'test/experiments/transport')
import transport_cut_setbacks as cuts
import transport_coupled_roadbed_experiment as coupled
exec(Path('test/experiments/transport/probes/roadbeds/coupled_deficit_probe.py').read_text().split('for module,pair in')[0])
source=inspect.getsource(cuts.setback_network).replace('not np.isfinite(distance) or distance<=0','any(not np.isfinite(d) or d<=0 for d in distance.values())').replace('displaced_source_section(network,pose,distance)','displaced_source_section(network,pose,distance[section])')
ns=dict(cuts.__dict__);exec(source,ns)
poses,_=net.poses(1565,{})
module_source=inspect.getsource(coupled.construct_module)
module_source=module_source.replace('    curves=[]','    original_floor=floor\n    curves=[]')
module_source=module_source.replace('    base,core,planned,physical,quantum=compose_roadbeds(floor,shape,', '        floor=floor.union(candidate["roadbed"])\n    base,core,planned,physical,quantum=compose_roadbeds(original_floor,shape,')
module_ns=dict(coupled.__dict__)
exec(module_source,module_ns)
coupled.construct_module=module_ns['construct_module']
shifts={poses[4]['section']:8.,poses[5]['section']:12.}
coupled.setback_network=lambda network,module,sections,distance:ns['setback_network'](network,module,sections,shifts)
output=root/'roadbeds/incremental-coupled-1565-probe'
report=coupled.real_case(root,name,1565,(4,5),8.,output)
report['setback_by_section_m']=shifts
report['shared_cuts_description']='Independent shared cuts moved 8 and 12 m; accumulated road space'
(output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda a:a.tolist())+'\n')
print(json.dumps({k:report[k] for k in ('previous_accepted','accepted','affected','added_area_m2')}))
