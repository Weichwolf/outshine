from pathlib import Path
import json,numpy as np,shapely
import infrastructure_corridor_experiment as corridors
import infrastructure_junction_turn_experiment as junctions
from infrastructure_flat_surface_network import load_graphs
from infrastructure_port_lanes import junction_interfaces,module_face_index,module_shape,lane_poses
from infrastructure_junction_surfaces import mesh_surface_index
root=Path('build/experiments/infrastructure')
vehicle=json.loads(Path('test/experiments/infrastructure/infrastructure_network_recipes.json').read_text())['vehicleProfiles']['compact']
case=next(r for r in json.loads((root/'port-frames'/'rotations.json').read_text()) if r['place']=='Wien' and r['module']==4528)
baseline=next(r for r in json.loads((root/'spline-hulls'/'corridors.json').read_text())['places'] if r['place']=='Wien')
module=next(m for m in baseline['modules'] if m['module']==case['module'])
position=np.array(case['positions'][1]);heading=np.deg2rad(case['fixed_heading_deg'][1])
rotation=np.deg2rad(-5.)
def overridden(*args,**kwargs):
    poses,unused=lane_poses(*args,**kwargs)
    for p in poses:
        actual=np.arctan2(p['heading'][1],p['heading'][0])
        if np.linalg.norm(p['position']-position)<1e-7 and abs(np.arctan2(np.sin(actual-heading),np.cos(actual-heading)))<1e-7:
            p['heading']=np.array([np.cos(actual+rotation),np.sin(actual+rotation)])
    return poses,unused

graphs=load_graphs(root/'directed-graphs/Wien-flat-network.npz',json.loads((root/'directed-graphs/Wien-flat-network.json').read_text()))
graph=next(g for g in graphs if g['tier']==['land',0,'ground']);tree=shapely.STRtree(graph['lines'])
with np.load(root/'directed-surfaces/Wien-flat-surfaces.npz') as archive:
    product={key:archive[key] for key in archive.files}
curbs=json.loads((root/'directed-surfaces/Wien-flat-surfaces.json').read_text())['curbs']
interfaces,faces,mesh=junction_interfaces(product),module_face_index(product),mesh_surface_index(product)
results=[];views=[];before_views=[]
def decisions(r):return {(t['entry'],t['exit']):t['accepted'] for t in r['turns']}
for parent in module['junctions']:
    shape=module_shape(product,parent,faces)
    poses,unused=lane_poses(graph,interfaces[parent],shape,vehicle,tree=tree)
    before,before_view=junctions.solve_module(parent,shape,poses,unused,graph,tree,mesh,vehicle,curbs)
    poses,unused=overridden(graph,interfaces[parent],shape,vehicle,tree=tree)
    after,view=junctions.solve_module(parent,shape,poses,unused,graph,tree,mesh,vehicle,curbs)
    old,new=decisions(before),decisions(after);assert old.keys()==new.keys()
    lost=[key for key,v in old.items() if v and not new[key]]
    gained=[key for key,v in new.items() if v and not old[key]]
    record=dict(junction=parent,before=before['accepted'],after=after['accepted'],requests=after['connections'],losses=lost,gains=gained)
    results.append(record);print(json.dumps(record),flush=True)
    before_views.append(('Wien vorher',parent,before,before_view))
    views.append(('Wien nachher',parent,after,view))
base_report,base_views=corridors.survey(root,'Wien',vehicle,96)
corridors.lane_poses=overridden
report,local=corridors.survey(root,'Wien',vehicle,96)
old={(m['module'],*key):v for m in baseline['modules'] for key,v in decisions(m).items()}
new={(m['module'],*key):v for m in report['modules'] for key,v in decisions(m).items()}
assert old.keys()==new.keys()
comparison=dict(before=baseline['counts'],after=report['counts'],losses=[key for key,v in old.items() if v and not new[key]],
                gains=[key for key,v in new.items() if v and not old[key]])
print('corridors',json.dumps(comparison),flush=True)
(root/'port-frames'/'coupled.json').write_text(json.dumps(dict(junctions=results,corridors=comparison),indent=2)+'\n')
v=next(v for v in base_views if v[1]==case['module']);before_views.insert(0,('Wien vorher',*v[1:]))
v=next(v for v in local if v[1]==case['module']);views.insert(0,('Wien nachher',*v[1:]))
views=before_views+views
junctions.render(Path('build/shots/experiments/infrastructure/coupled-port-frame.png'),views,
                 title='One shared port-frame rotation: neighboring junctions and corridor checked together')
