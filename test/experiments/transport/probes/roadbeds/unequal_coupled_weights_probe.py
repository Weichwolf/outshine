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
rows=[]
import transport_hermite_route_fitting as fitting
fit_source=inspect.getsource(fitting.fit_hermite_route)
for weight in (8,16,32):
 left,right=8.,12.
 fit_ns=dict(fitting.__dict__)
 exec(fit_source.replace('bends = 4*',f'bends = {weight}*'),fit_ns)
 fitting.fit_hermite_route=fit_ns['fit_hermite_route']
 shifts={poses[4]['section']:left,poses[5]['section']:right}
 coupled.setback_network=lambda network,module,sections,distance:ns['setback_network'](network,module,sections,shifts)
 output=root/f'roadbeds/unequal-coupled-1565-{left:g}-{right:g}-weight-{weight}-probe'
 try:
  report=coupled.real_case(root,name,1565,(4,5),left,output)
  report['setback_by_section_m']=shifts
  report['shared_cuts_description']=f'Independent shared cuts moved {left:g} and {right:g} m; all affected routes checked'
  report['scope']='Unintegrated unequal-cut experiment; no runtime integration'
  (output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda a:a.tolist())+'\n')
  row=dict(weight=weight,offsets=[left,right],success=True,**{k:report[k] for k in ('previous_accepted','accepted','affected','added_area_m2')})
 except (AssertionError,cuts.CutSetbackRejected) as error:
  row=dict(weight=weight,offsets=[left,right],accepted=False,reason=str(error))
 rows.append(row);print(json.dumps(row),flush=True)
Path('build/experiments/transport/roadbeds/unequal_coupled_weights_probe.json').write_text(json.dumps(rows,indent=2)+'\n')
