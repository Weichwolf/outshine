from pathlib import Path
import json,sys
import numpy as np,shapely
sys.path.insert(0,'test/experiments/transport')
import transport_coupled_roadbed_experiment as coupled
from transport_turn_roadbeds import bounded_voids,compose_roadbeds
original=coupled.coupled_addition_rejection
rows=[]
def capture(floor,shape,supports,reserved,section_index,trial,owner,affected):
 reason=original(floor,shape,supports,reserved,section_index,trial,owner,affected)
 if reason=='combined_void_topology':
  base,core,planned,physical,quantum=compose_roadbeds(floor,shape,supports)
  row=dict(owner=owner,base_voids=shapely.area(bounded_voids(base)).tolist(),planned_voids=shapely.area(bounded_voids(planned)).tolist(),core_voids=shapely.area(bounded_voids(core)).tolist(),physical_voids=shapely.area(bounded_voids(physical)).tolist(),base_parts=len(shapely.get_parts(base)),planned_parts=len(shapely.get_parts(planned)),core_parts=len(shapely.get_parts(core)),physical_parts=len(shapely.get_parts(physical)),base_wkb=base.wkb_hex,core_wkb=core.wkb_hex,planned_wkb=planned.wkb_hex,physical_wkb=physical.wkb_hex,reserved_wkb=reserved.wkb_hex,support_wkb=supports[-1].wkb_hex)
  rows.append(row);print(json.dumps({k:v for k,v in row.items() if not k.endswith('_wkb')}),flush=True)
 return reason
coupled.coupled_addition_rejection=capture
try:coupled.real_case(Path('build/experiments/transport'),'ZuerichHauptbahnhof',1704,(3,5),4.,Path('build/experiments/transport/roadbeds/1704-void-diagnostic'),[11180])
except AssertionError as error:print(str(error),flush=True)
Path('build/experiments/transport/roadbeds/1704_void_diagnostic.json').write_text(json.dumps(rows,indent=2)+'\n')
