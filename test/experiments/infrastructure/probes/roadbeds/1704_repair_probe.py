from pathlib import Path
import json,sys
import numpy as np,shapely
sys.path.insert(0,'test/experiments/infrastructure')
import infrastructure_coupled_roadbed_experiment as coupled
from infrastructure_turn_roadbeds import bounded_voids,coordinate_roundoff
original=coupled.compose_roadbeds

def repair(before,after):
 old=shapely.STRtree(bounded_voids(before));patches=[]
 for hole in bounded_voids(after):
  if any(hole.intersection(previous).area>0 for previous in old.geometries[old.query(hole,predicate='intersects')]):continue
  origin=shapely.get_coordinates(hole)[0]
  rectangle=shapely.minimum_rotated_rectangle(shapely.transform(hole,lambda points:points-origin))
  points=shapely.get_coordinates(rectangle)
  width=np.linalg.norm(np.diff(points,axis=0),axis=1).min() if rectangle.geom_type=='Polygon' else 0.
  if width<=coordinate_roundoff(before):patches.append(hole)
 return after if not patches else shapely.union_all([after,*patches])

def compose(floor,shape,supports):
 base,core,planned,physical,precision=original(floor,shape,supports)
 planned=repair(base,planned)
 physical=core.union(planned.difference(base));fixed=repair(core,physical)
 planned=planned.union(fixed.difference(physical))
 return base,core,planned,fixed,precision
coupled.compose_roadbeds=compose
output=Path('build/experiments/infrastructure/roadbeds/1704-repair-probe')
report=coupled.real_case(Path('build/experiments/infrastructure'),'ZuerichHauptbahnhof',1704,(3,5),4.,output,[11180])
report['scope']='Unintegrated numerical-hole repair experiment; not a runtime product'
(output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda obj:obj.tolist())+'\n')
print(json.dumps({k:report[k] for k in ('previous_accepted','accepted','connections','added_area_m2')}))
