from pathlib import Path
import json,numpy as np,shapely
import transport_corridor_experiment as corridors
from transport_flat_ports import shared_port_sections
from transport_directed_routes import directed_module_routes
from transport_junction_turns import curvature_connector
from transport_vehicle_sweep import body_outlines
root=Path('build/experiments/transport')
vehicle=json.loads(Path('test/experiments/transport/transport_network_recipes.json').read_text())['vehicleProfiles']['compact']
original=corridors.solve_module
records=[]

def inspect(module,shape,poses,unused,graph,tree,mesh,profile,curbs,**kw):
    result=original(module,shape,poses,unused,graph,tree,mesh,profile,curbs,**kw)
    if result[0]['accepted']==result[0]['connections']:return result
    surface=result[1][1]
    routes,_=directed_module_routes(graph,poses,shape,tree)
    for note in result[0]['turns']:
        if note['accepted']:continue
        i,j=note['entry'],note['exit'];entry,exit=poses[i],poses[j]
        directions={}
        for index,pose in ((i,entry),(j,exit)):
            section=pose['section'];line=sections['end'][section]-sections['begin'][section]
            normal=np.array([-line[1],line[0]])/np.linalg.norm(line)
            if np.dot(normal,pose['heading'])<0:normal=-normal
            directions[index]=normal
        candidates=[]
        for a in ('fixed','normal'):
            for b in ('fixed','normal'):
                first=dict(entry,heading=entry['heading'] if a=='fixed' else directions[i])
                last=dict(exit,heading=exit['heading'] if b=='fixed' else directions[j])
                path,report=curvature_connector(first,last,surface,profile,.025)
                rotation=[float(np.rad2deg(np.arctan2(p['heading'][0]*q['heading'][1]-p['heading'][1]*q['heading'][0],np.dot(p['heading'],q['heading']))))
                          for p,q in ((entry,first),(exit,last))]
                candidates.append(dict(frames=[a,b],rotations_deg=rotation,accepted=path is not None,
                                       reason=report.get('reason'),radius_m=report.get('minimum_radius_m')))
        route=routes[i,j]
        row=dict(place=name,module=int(module),entry=i,exit=j,source_length_m=route.length,
                 source_points=shapely.get_coordinates(route).tolist(),positions=[entry['position'].tolist(),exit['position'].tolist()],
                 fixed_heading_deg=[float(np.rad2deg(np.arctan2(p['heading'][1],p['heading'][0]))) for p in (entry,exit)],
                 candidates=candidates)
        records.append(row);print(json.dumps({k:v for k,v in row.items() if k!='source_points'}),flush=True)
    return result
corridors.solve_module=inspect
for name in ('Wien','ZuerichHauptbahnhof'):
    with np.load(root/'directed-surfaces'/f'{name}-flat-surfaces.npz') as archive:
        sections=shared_port_sections({k:archive[k] for k in archive.files})
    corridors.survey(root,name,vehicle,96)
(root/'port-frames'/'failures.json').write_text(json.dumps(records,indent=2)+'\n')
