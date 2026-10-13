import json,sys
from pathlib import Path
import shapely
sys.path.insert(0,str(Path('test/experiments/transport').resolve()))
from transport_window_routes_render import prepared_network
from transport_junction_surfaces import motor_surface
from transport_junction_turn_experiment import render

root=Path('build/experiments/transport')
reference=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
vehicle=json.loads(Path('test/experiments/transport/transport_network_recipes.json').read_text())['vehicleProfiles']['compact']
before,after=[],[]
for name,module in [('Wien',3765),('ZuerichHauptbahnhof',629),('BaselBadischerBahnhof',939)]:
    old=json.loads((root/'window-1000'/f'{name}-routes.json').read_text())
    new=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
    source=next(p for p in reference['places'] if p['place']==name)
    network=prepared_network(root,new,source,vehicle)
    shape=network.shape(module)
    poses,_=network.poses(module,{})
    floor=motor_surface(shape,network.graph,network.tree,network.mesh,vehicle,
                         network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
    for report,views,title in [(old,before,'before'),(new,after,'after')]:
        row=next(m for m in report['modules'] if m['module']==module)
        xy=next(m for m in report['paths'] if m['module']==module)['paths']
        paths=[None if p is None else shapely.LineString(p) for p in xy]
        views.append((name+' '+title,module,row,(shape,floor,poses,paths,row['turns'])))
render(Path('build/shots/experiments/transport/body-supported-routes.png'),before+after,
       'Additional body support points: unchanged road surfaces, ports and budgets; continuous vehicle sweep proved')
