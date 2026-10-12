import pickle,sys,json,inspect
from pathlib import Path
import numpy as np,shapely
sys.path.insert(0,'test/experiments/transport')
import transport_coupled_roadbed_experiment as coupled
from transport_source_roadbeds import sample_roadbed_piece
from transport_vehicle_sweep import body_outlines
products,trial,precision=pickle.loads(Path('build/experiments/transport/roadbeds/basel396-partial-coupled-probe/publication-input.pickle').read_bytes())
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
source=inspect.getsource(coupled.publish_coupled_roadbeds).replace("            assert body['fits_surface'],(owner,curve['pair'],body)","            if not body['fits_surface']:diagnose(owner,curve,complete,body)")
ns=dict(coupled.__dict__,diagnose=diagnose);exec(source,ns)
ns['publish_coupled_roadbeds'](products,trial,precision)
Path('build/experiments/transport/roadbeds/basel396_mesh_diagnostic.json').write_text(json.dumps(failures,indent=2)+'\n')
