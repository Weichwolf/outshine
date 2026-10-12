import json
import sys
import time
from pathlib import Path

import numpy as np
import shapely

sys.path.insert(0,str(Path('test/experiments/transport').resolve()))
from transport_cut_setback_experiment import prepared_network,digest
from transport_junction_surfaces import motor_surface,same_island_winding
from transport_junction_turns import tangent_intersection,connect_lanes
from transport_directed_routes import directed_module_routes
from transport_flat_corners import corner_patches
from transport_clothoid_turns import fit_symmetric_turns,sample_symmetric_turns
from transport_vehicle_sweep import turn_body_check
from transport_travel import MODES

root = Path('build/experiments/transport')
base = json.loads((root/'window-1000-body/BaselBadischerBahnhof-routes.json').read_text())
frames = json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
n = prepared_network(root,base,next(p['frames'] for p in frames['places'] if p['place']=='BaselBadischerBahnhof'))
shape = n.shape(1109)
poses,_ = n.poses(1109,{})
original = motor_surface(shape,n.graph,n.tree,n.mesh,n.vehicle,n.curbs['corner_radius_m'],n.curbs['arc_error_m'])
routes,_ = directed_module_routes(n.graph,poses,shape,n.tree)
entry,exit = poses[50],poses[55]
node,reason = tangent_intersection(entry,exit)
assert reason is None
bounds = shapely.box(*(node-24),*(node+24))
edges = n.tree.query(bounds,predicate='intersects')
edges = edges[np.any(n.graph['travel_public'][edges]&MODES['motor']!=0,axis=1)]
raw = shapely.union_all(shapely.buffer(n.graph['lines'][edges],n.graph['half_widths'][edges],quad_segs=2,cap_style='round',join_style='round'))
rows = []
for radius in (1.8,2.7,3.3,4.,5.,6.,8.):
    started = time.perf_counter()
    polygons = []
    for polygon in shapely.get_parts(raw):
        patches,_ = corner_patches(shapely.get_coordinates(polygon.exterior),radius,.025,True,1e-7)
        for points in patches:
            patch = shapely.Polygon(points)
            if patch.is_valid and patch.distance(shapely.Point(node))<=n.vehicle['minimumRearAxleRadiusM']:
                polygons.append(patch)
    added = shapely.union_all(polygons)
    floor = shapely.union_all([original,added])
    path,note = connect_lanes(entry,exit,floor,n.vehicle,.025)
    if path is None:
        h0,h1 = (np.array([np.arctan2(p['heading'][1],p['heading'][0])]) for p in (entry,exit))
        for transition in (.5,.25,.125):
            unit = fit_symmetric_turns(node,np.array([1.]),h0,h1,transition_fraction=transition)
            trim = float(unit['maximum_curvature_per_m'][0])*n.vehicle['minimumRearAxleRadiusM']
            available = min(np.linalg.norm(node-entry['position']),np.linalg.norm(exit['position']-node))
            if trim>available:
                continue
            fit = fit_symmetric_turns(node,np.array([trim]),h0,h1,transition_fraction=transition)
            body,_ = turn_body_check(floor,fit,entry['position'],exit['position'],'G2',n.vehicle,.025)
            if body['fits_surface']:
                xy = sample_symmetric_turns(fit,np.linspace(0,1,129))[0][0]
                points = np.vstack((entry['position'],xy,exit['position']))
                points = points[np.r_[True,np.linalg.norm(np.diff(points,axis=0),axis=1)>1e-10]]
                path = shapely.LineString(points)
                note = dict(profile='radius-boundary-G2',transition_fraction=transition,vehicle_sweep=body,
                            minimum_radius_m=1/float(fit['maximum_curvature_per_m'][0]))
                break
    winding = path is not None and same_island_winding(path,routes[50,55],original)
    row = dict(corner_radius_m=radius,patches=len(polygons),added_area_m2=shapely.difference(floor,original).area,
               accepted=path is not None and winding,elapsed_ms=(time.perf_counter()-started)*1000,note=note)
    if row['accepted']:
        row.update(path=shapely.get_coordinates(path).tolist(),floor_wkb=shapely.to_wkb(floor,hex=True),patch_wkb=shapely.to_wkb(added,hex=True))
    rows.append(row)
    print(json.dumps({k:v for k,v in row.items() if k not in ('path','floor_wkb','patch_wkb','note')}),flush=True)
Path('build/experiments/transport/cut-setbacks/corner-flare-results.json').write_text(json.dumps(dict(results=rows,
    prototype_sha256=digest(Path(__file__)),input_sha256=base['input_sha256']),indent=2)+'\n')
