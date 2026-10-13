import sys,json
from pathlib import Path
import numpy as np,shapely
sys.path.insert(0,'test/experiments/transport')
from transport_cut_setback_experiment import prepared_network
from transport_cut_setbacks import restore_cut_plan
from transport_directed_routes import directed_module_routes
from transport_junction_surfaces import motor_surface
from transport_vehicle_sweep import body_outlines

root=Path('build/experiments/transport'); name='ZuerichHauptbahnhof'
before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
net=prepared_network(root,before,next(x['frames'] for x in frames['places'] if x['place']==name))
restore_cut_plan(net,json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text())['plan'])
for module,pair in ((1453,(3,1)),(1565,(4,5)),(1704,(0,5)),(1706,(4,0))):
    poses,_=net.poses(module,{})
    routes,_=directed_module_routes(net.graph,poses,net.shape(module),net.tree)
    domain=shapely.union_all(list(routes.values())).envelope
    floor=motor_surface(domain,net.graph,net.tree,net.mesh,net.vehicle,net.curbs['corner_radius_m'],net.curbs['arc_error_m'])
    rows=[]
    for i in pair:
        p=poses[i]; axis=net.graph['lines'][p['edge']]
        sign=p['direction']*(-1 if p['incoming'] else 1)
        reach=axis.length-p['station'] if sign>0 else p['station']
        angle=np.arctan2(p['heading'][1],p['heading'][0])
        outline=body_outlines(p['position'][None,:],np.array([angle]),net.vehicle['widthM']*.5,
                             net.vehicle['frontFromRearAxleM'],net.vehicle['rearOverhangM'])[0]
        rows.append(dict(index=i,section=p['section'],edge=p['edge'],corridor=p['corridor'],
            station=p['station'],direction=sign,source_length=axis.length,reach_m=reach,
            body_outside_m2=shapely.Polygon(outline).difference(floor).area))
    print(json.dumps(dict(module=module,ports=len(poses),routes=len(routes),
                         area_m2=net.shape(module).area,selected=rows)),flush=True)
