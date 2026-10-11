import pickle,sys,json,inspect
from pathlib import Path
import numpy as np,shapely
sys.path.insert(0,'test/experiments/infrastructure')
import infrastructure_coupled_roadbed_experiment as coupled
from infrastructure_source_roadbeds import sample_roadbed_piece
from infrastructure_vehicle_sweep import body_outlines
products,trial,precision=pickle.loads(Path('build/experiments/infrastructure/roadbeds/basel396-partial-coupled-probe/publication-input.pickle').read_bytes())
failures=[]
def diagnose(owner,curve,complete,body):
 outside=[]
 for piece in curve['candidate']['pieces']:
  xy,angles=sample_roadbed_piece(piece,np.linspace(0,1,1001))
  rectangles=shapely.polygons(body_outlines(xy,angles,trial.vehicle['widthM']*.5,trial.vehicle['frontFromRearAxleM'],trial.vehicle['rearOverhangM']))
  lost=shapely.difference(rectangles,complete)
  outside.append(float(shapely.area(lost).max()))
 row=dict(owner=owner,pair=curve['pair'],profile=curve['candidate']['pieces'][0]['profile'],existing=curve['existing_surface'],max_sampled_outside_m2=max(outside),floor_lost_m2=products[owner]['planned'].difference(complete).area,coordinate_precision_m=products[owner]['quantum'],body=body)
 failures.append(row);print(json.dumps(row),flush=True)
original=coupled.roadbed_body_check
maximum_roundoff=0.
def robust(surface,pieces,vehicle,error):
 global maximum_roundoff
 scale=max(1.,float(np.abs(shapely.get_coordinates(surface)).max(initial=0)))
 roundoff=64*np.finfo(float).eps*scale
 maximum_roundoff=max(maximum_roundoff,roundoff)
 return original(shapely.buffer(surface,roundoff,quad_segs=8),pieces,vehicle,error)
coupled.roadbed_body_check=robust
mesh,audit,proofs,surfaces=coupled.publish_coupled_roadbeds(products,trial,precision)
print(json.dumps(dict(proved_routes=len(proofs),coordinate_roundoff_m=maximum_roundoff,owners=len(products))))
