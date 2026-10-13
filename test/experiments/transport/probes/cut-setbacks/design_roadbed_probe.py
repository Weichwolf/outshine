import hashlib
import json
import math
from pathlib import Path
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path('test/experiments/transport').resolve()))
from transport_cut_setback_experiment import prepared_network,digest
from transport_junction_surfaces import motor_surface,same_island_winding
from transport_junction_turns import tangent_intersection
from transport_directed_routes import directed_module_routes
from transport_clothoid_turns import fit_symmetric_turns,sample_symmetric_turns
from transport_vehicle_sweep import body_outlines,turn_body_check
from transport_building_clearance import features,polygon_parts,GIRTH_M
from transport_travel import MODES

root=Path('build/experiments/transport')
name='BaselBadischerBahnhof'
base=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
n=prepared_network(root,base,next(p['frames'] for p in frames['places'] if p['place']==name))
shape=n.shape(1109)
poses,_=n.poses(1109,{})
floor=motor_surface(shape,n.graph,n.tree,n.mesh,n.vehicle,n.curbs['corner_radius_m'],n.curbs['arc_error_m'])
routes,_=directed_module_routes(n.graph,poses,shape,n.tree)
vehicle=n.vehicle
error=n.curbs['arc_error_m']
results=[]
patches=[]
for a,b in ((50,55),(56,49)):
    started=time.perf_counter()
    entry,exit=poses[a],poses[b]
    node,reason=tangent_intersection(entry,exit)
    assert reason is None
    h0,h1=(np.array([np.arctan2(p['heading'][1],p['heading'][0])]) for p in (entry,exit))
    unit=fit_symmetric_turns(node,np.array([1.]),h0,h1,transition_fraction=.25)
    trim=float(unit['maximum_curvature_per_m'][0])*vehicle['minimumRearAxleRadiusM']*(1+1e-9)
    fit=fit_symmetric_turns(node,np.array([trim]),h0,h1,transition_fraction=.25)
    curvature=float(fit['maximum_curvature_per_m'][0])
    length=float(fit['length_m'][0])
    rate=float(fit['maximum_curvature_rate_per_m2'][0])
    radius=np.hypot(vehicle['frontFromRearAxleM'],vehicle['widthM']*.5)
    acceleration=(curvature+radius*(rate+curvature**2))*length**2
    segments=int(np.ceil(np.sqrt(acceleration/(8*error))))
    positions,headings=sample_symmetric_turns(fit,np.linspace(0,1,segments+1))[:2]
    positions,headings=positions[0],headings[0]
    outlines=body_outlines(positions,headings,vehicle['widthM']*.5,vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
    hulls=shapely.convex_hull(shapely.multipoints(np.concatenate((outlines[:-1],outlines[1:]),axis=1)))
    curved=shapely.buffer(hulls,(acceleration/(8*segments**2)+2*error)/np.cos(np.pi/32),quad_segs=8)
    approaches=[]
    for p,q,angle in ((entry['position'],positions[0],headings[0]),(positions[-1],exit['position'],headings[-1])):
        bodies=body_outlines(np.array([p,q]),np.full(2,angle),vehicle['widthM']*.5,vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
        hull=shapely.convex_hull(shapely.MultiPoint(bodies.reshape(-1,2)))
        approaches.append(shapely.buffer(hull,2*error/np.cos(np.pi/32),quad_segs=8))
    envelope=shapely.union_all([*curved,*approaches])
    patch=shapely.difference(envelope,floor)
    planned=shapely.union_all([floor,envelope])
    body,_=turn_body_check(planned,fit,entry['position'],exit['position'],'G2',vehicle,error)
    dense_xy,dense_heading=sample_symmetric_turns(fit,np.linspace(0,1,2049))[:2]
    dense_bodies=shapely.polygons(body_outlines(dense_xy[0],dense_heading[0],vehicle['widthM']*.5,vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']))
    outside=shapely.union_all(shapely.difference(dense_bodies,floor))
    path=shapely.LineString(np.vstack((entry['position'],dense_xy[0],exit['position'])))
    near=n.tree.query(shapely.buffer(patch,float(n.graph['half_widths'].max())),predicate='intersects')
    private=near[~np.any(n.graph['travel_public'][near]&MODES['motor']!=0,axis=1)]
    reserved=shapely.union_all(shapely.buffer(n.graph['lines'][private],n.graph['half_widths'][private],quad_segs=2)).difference(floor)
    holes=shapely.union_all([shapely.Polygon(r) for p in shapely.get_parts(floor) for r in p.interiors])
    row=dict(entry=a,exit=b,segments=segments,patch_m2=patch.area,outside_body_m2=outside.area,patch_bounds=patch.bounds,
             reserved_overlap_m2=patch.intersection(reserved).area,hole_overlap_m2=patch.intersection(holes).area,
             maximum_outside_body_distance_m=float(shapely.distance(shapely.points(shapely.get_coordinates(outside)),floor).max(initial=0)),
             body=body,dense_bodies_covered=bool(np.all(shapely.covers(planned,dense_bodies))),
             winding=same_island_winding(path,routes[a,b],floor),elapsed_ms=(time.perf_counter()-started)*1000)
    patches.append(patch)
    results.append(row)
    print(json.dumps(row),flush=True)
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
lat,lon=receipt['origin']
scale=math.cos(math.radians(lat))
anchor=np.array([lon/360*GIRTH_M,math.asinh(math.tan(math.radians(lat)))*GIRTH_M/(2*math.pi)])
region=shapely.buffer(shapely.union_all(patches),10)
buildings=[]
for tile in receipt['source_tiles']:
    z,x,y=tile['tile']
    span=GIRTH_M/(1<<z)
    corner=(np.array([x*span,-y*span])+[-GIRTH_M/2,GIRTH_M/2]-anchor)*scale
    if not shapely.box(corner[0],corner[1]-span*scale,corner[0]+span*scale,corner[1]).intersects(region):
        continue
    data=Path(tile['path']).read_bytes()
    assert hashlib.sha256(data).hexdigest()==tile['sha256']
    for layer,extent,kind,props,parts in features(data):
        if layer!='building' or kind!=3:
            continue
        rings=[((np.asarray(p)/extent+[x,y])*[span,-span]+[-GIRTH_M/2,GIRTH_M/2]-anchor)*scale for p in parts]
        for poly in polygon_parts(rings):
            if poly.is_valid and poly.intersects(region):
                buildings.append(poly)
reserved=shapely.union_all(buildings)
for row,patch in zip(results,patches):
    row['building_overlap_m2']=patch.intersection(reserved).area
    row['nearest_building_m']=patch.distance(reserved) if not reserved.is_empty else None
    print('building reservation',row['entry'],row['exit'],row['building_overlap_m2'],row['nearest_building_m'],flush=True)
Path('build/experiments/transport/cut-setbacks/design-roadbed-results.json').write_text(json.dumps(dict(results=results,prototype_sha256=digest(Path(__file__)),input_sha256=base['input_sha256']),indent=2)+'\n')

near=n.tree.query(region,predicate='intersects')
public=near[np.any(n.graph['travel_public'][near]&MODES['motor']!=0,axis=1)]
bands=shapely.union_all(shapely.buffer(n.graph['lines'][public],n.graph['half_widths'][public],quad_segs=2))
triangles,index=n.mesh
physical=shapely.union_all(triangles[index.query(region,predicate='intersects')])
for part in shapely.get_parts(floor):
    for ring in part.interiors:
        hole=shapely.Polygon(ring)
        if hole.intersects(shapely.union_all(patches)):
            print('hole diagnostic',dict(area=hole.area,bounds=hole.bounds,source_band_coverage=hole.intersection(bands).area/hole.area,physical_coverage=hole.intersection(physical).area/hole.area,source_route_crossings={str(k):v.intersects(hole) for k,v in routes.items() if k in ((50,55),(56,49))}),flush=True)
for patch in patches:
    ids=n.mesh[1].query(patch,predicate='intersects')
    areas=shapely.area(shapely.intersection(n.mesh[0][ids],patch))
    owners=n.product['face_owners'][ids]
    print('physical ownership',dict(owner_area={str(int(o)):float(areas[owners==o].sum()) for o in np.unique(owners) if areas[owners==o].sum()>1e-8},core_coverage=patch.intersection(shape).area),flush=True)
