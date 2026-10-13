"""Compare bounded return-turn seeds with actual public road space and reservations."""

import argparse
import json
from itertools import chain
from pathlib import Path
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from transport_cut_setback_experiment import prepared_network
from transport_cut_setbacks import restore_cut_plan,setback_network
from transport_directed_routes import directed_module_routes
from transport_fitted_roadbeds import construct_fitted_roadbed,spline_roadbed
from transport_hermite_route_fitting import fit_hermite_route
from transport_junction_surfaces import motor_surface,same_island_winding
from transport_three_arc_paths import three_arc_seeds
from transport_arc_line_paths import arc_line_arc_seeds
from transport_turn_roadbed_experiment import building_reservations,reservations,mesh_coverage
from transport_turn_roadbeds import roadbed_lateral_reach,roadbed_site_check,geometry_precision

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--search-reach',type=float)
parser.add_argument('--sections',type=int,nargs='+')
args=parser.parse_args()
root=Path('build/experiments/transport');name='ZuerichHauptbahnhof';module=1678
suffix='' if args.search_reach is None else f'-reach-{args.search_reach:g}'
if args.sections:suffix+='-sections-'+ '-'.join(map(str,args.sections))
output=root/('roadbeds/return-turn-families-1678'+suffix);output.mkdir(parents=True,exist_ok=True)
before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
restore_cut_plan(network,json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text())['plan'])
poses,_=network.poses(module,{})
sections={poses[i]['section'] for i in (4,6)} if args.sections is None else set(args.sections)
network,_,affected=setback_network(network,module,sections,4.)
poses,_=network.poses(module,{})
routes,_=directed_module_routes(network.graph,poses,network.shape(module),network.tree)
error=network.curbs['arc_error_m'];vehicle=network.vehicle
floor=motor_surface(shapely.union_all(list(routes.values())).envelope,network.graph,network.tree,network.mesh,
                    vehicle,network.curbs['corner_radius_m'],error)
region=shapely.union_all([network.shape(m) for m in affected]).envelope.buffer(roadbed_lateral_reach(vehicle)+error)
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
buildings,_=building_reservations(receipt,region)
reserved=buildings.union(reservations(network,region).difference(floor))
reach=roadbed_lateral_reach(vehicle) if args.search_reach is None else args.search_reach
if reach<=0:raise ValueError('search reach must be positive')
domain=floor.buffer(reach).difference(reserved.difference(floor))
results=[]
for pair in ((4,6),(5,3),(5,6)):
    options=[]
    for radius in (4.2,4.5,5.):
        families=chain(((('CCC',*kind),seed) for kind,seed in three_arc_seeds(poses[pair[0]],poses[pair[1]],radius)),
                       ((('CSC',*kind),seed) for kind,seed in arc_line_arc_seeds(poses[pair[0]],poses[pair[1]],radius)))
        for kind,seed in families:
            if not same_island_winding(seed,routes[pair],floor):continue
            outside=seed.buffer(vehicle['widthM']*.5).difference(domain).area
            options.append((outside,seed.length,radius,kind,seed))
    for rank,(outside,length,radius,kind,seed) in enumerate(sorted(options,key=lambda row:row[:2])[:2]):
        began=time.perf_counter()
        candidate,note=construct_fitted_roadbed(poses[pair[0]],poses[pair[1]],floor,reserved,seed,vehicle,error,maximum_calls=512)
        row=dict(pair=pair,rank=rank,seed_radius_m=radius,seed_kind=kind,seed_length_m=length,
                 axis_buffer_outside_m2=outside,elapsed_ms=(time.perf_counter()-began)*1000,
                 accepted=candidate is not None,reason=note['reason'],residual_calls=note['residual_calls'])
        if candidate is not None:
            assert same_island_winding(candidate['path'],routes[pair],floor)
            mesh_coverage(candidate['surface'],vehicle,candidate['pieces'],error)
            row.update(path=shapely.get_coordinates(candidate['path']).tolist(),added_area_m2=candidate['addition'].area,
                       surface_wkb=candidate['surface'].wkb.hex())
        results.append(row);print(json.dumps({k:v for k,v in row.items() if k not in ('path','surface_wkb')}),flush=True)
        for spacing in (8.,4.,2.):
            began=time.perf_counter()
            path,fit=fit_hermite_route(poses[pair[0]],poses[pair[1]],seed,domain,vehicle,error,
                                     spacing=spacing,maximum_calls=512,include_controls=True)
            trial=dict(pair=pair,rank=rank,spacing_m=spacing,accepted=False,curve_found=path is not None,
                       residual_calls=fit['residual_calls'],reason=fit.get('reason'),
                       objective=fit['minimum_objective_seen'])
            if path is not None:
                controls=np.array(fit['curve_controls'])
                bed,audit=spline_roadbed(controls,vehicle,error)
                if bed is not None:
                    base,protect,bed=shapely.set_precision(np.array([floor,reserved,bed],dtype=object),geometry_precision(floor))
                    planned,addition,reason=roadbed_site_check(bed.difference(protect),base,protect,vehicle,error)
                    trial.update(accepted=reason is None,reason=reason,added_area_m2=addition.area)
                    if reason is None:
                        assert same_island_winding(path,routes[pair],floor)
                        piece=dict(profile='G2-quintic-spline',fit=dict(controls=controls),
                                   begin=poses[pair[0]]['position'],end=poses[pair[1]]['position'])
                        mesh_coverage(planned,vehicle,[piece],error)
                        trial.update(path=shapely.get_coordinates(path).tolist(),surface_wkb=planned.wkb.hex())
                else:trial['reason']=audit['reason']
            trial['elapsed_ms']=(time.perf_counter()-began)*1000
            results.append(trial);print(json.dumps({k:v for k,v in trial.items() if k not in ('path','surface_wkb')}),flush=True)
(output/'results.json').write_text(json.dumps(results,indent=2)+'\n')

from transport_building_clearance import plot_geometry
import matplotlib.pyplot as plt
fig,axes=plt.subplots(1,3,figsize=(15,6),constrained_layout=True)
for pair,ax in zip(((4,6),(5,3),(5,6)),axes):
    plot_geometry(ax,[floor],'#8d9993')
    choices=[r for r in results if tuple(r['pair'])==pair and r['accepted']]
    if choices:
        best=min(choices,key=lambda r:r['added_area_m2'])
        surface=shapely.from_wkb(best['surface_wkb'])
        plot_geometry(ax,[surface.difference(floor)],'#71bd94')
        xy=np.array(best['path']);ax.plot(xy[:,0],xy[:,1],color='#286b9e')
    xy=shapely.get_coordinates(routes[pair]);ax.plot(xy[:,0],xy[:,1],'--',color='#b54a49')
    a,b,c,d=routes[pair].bounds;ax.set_xlim(a-12,c+6);ax.set_ylim(b-10,d+10)
    ax.set_aspect('equal');ax.set_title(f'{pair}: {len(choices)} valid local candidates')
fig.savefig(output/'return-turns.png',dpi=150)
