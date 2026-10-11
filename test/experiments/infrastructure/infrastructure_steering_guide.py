"""Search bounded forward steering states before conservative G2 refinement."""

import heapq
from itertools import chain

import numpy as np
import shapely

from infrastructure_arc_line_paths import arc_line_arc_seeds
from infrastructure_three_arc_paths import three_arc_seeds
from infrastructure_vehicle_sweep import body_outlines


def steering_points(pose,turn,length,radius):
    samples=np.linspace(0,length,3)
    x,y,angle=pose
    if turn==0:
        angles=np.full(3,angle)
        points=np.c_[x+samples*np.cos(angle),y+samples*np.sin(angle)]
    else:
        curvature=turn/radius;angles=angle+curvature*samples
        points=np.c_[x+(np.sin(angles)-np.sin(angle))/curvature,
                     y+(np.cos(angle)-np.cos(angles))/curvature]
    return points,angles


def crossing_signature(points,markers):
    if not len(markers):return ()
    a=points[:-1,None,:]-markers
    b=points[1:,None,:]-markers
    cross=a[:,:,0]*b[:,:,1]-a[:,:,1]*b[:,:,0]
    positive=(a[:,:,1]<=0)&(b[:,:,1]>0)&(cross>0)
    negative=(a[:,:,1]>0)&(b[:,:,1]<=0)&(cross<0)
    return tuple((positive.sum(axis=0)-negative.sum(axis=0)).tolist())


def bodies_fit(points,angles,domain,vehicle):
    bodies=body_outlines(points,angles,vehicle['widthM']*.5,
                        vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
    return bool(np.all(shapely.covers(domain,shapely.polygons(bodies))))


def connector_paths(pose,exit,radius):
    entry=dict(position=np.array(pose[:2]),heading=np.array([np.cos(pose[2]),np.sin(pose[2])]))
    return chain((path for _,path in arc_line_arc_seeds(entry,exit,radius)),
                 (path for _,path in three_arc_seeds(entry,exit,radius)))


def steering_guide(entry,exit,source,surface,domain,vehicle,*,maximum_states=20000,heading_bins=32,cell=None,radius_scale=1.05):
    if maximum_states<1 or heading_bins<8:raise ValueError('steering budgets must be positive')
    if radius_scale<1 or not np.isfinite(radius_scale):raise ValueError('guide radius cannot undercut the vehicle radius')
    radius=vehicle['minimumRearAxleRadiusM']*radius_scale
    angle_step=2*np.pi/heading_bins;length=radius*angle_step
    cell=vehicle['widthM']*.25 if cell is None else cell
    if cell<=0 or not np.isfinite(cell):raise ValueError('state cell must be finite and positive')
    region=source.envelope.buffer(radius*2+vehicle['frontFromRearAxleM'])
    domain=domain.intersection(region);shapely.prepare(domain)
    holes=[ring for part in shapely.get_parts(surface) for ring in part.interiors]
    markers=shapely.get_coordinates(shapely.point_on_surface(shapely.polygons(holes))) if holes else np.empty((0,2))
    markers=markers[shapely.covers(region,shapely.points(markers))]
    wanted=crossing_signature(shapely.get_coordinates(source),markers)
    origin=entry['position'];angle=np.arctan2(*entry['heading'][::-1])
    initial=np.r_[origin,angle];zero=tuple(0 for _ in markers)
    states=[dict(pose=initial,cost=0.,signature=zero,parent=None,points=origin[None,:])]
    key=lambda p,s:(*np.rint((p[:2]-origin)/cell).astype(int),int(np.rint((p[2]-angle)/angle_step))%heading_bins,s)
    best={key(initial,zero):0.};queue=[(np.linalg.norm(origin-exit['position']),0)]
    expanded,connectors=0,0
    while queue and expanded<maximum_states:
        _,index=heapq.heappop(queue);node=states[index];pose=node['pose']
        if node['cost']>best[key(pose,node['signature'])]+1e-10:continue
        expanded+=1
        if np.linalg.norm(pose[:2]-exit['position'])<=2*radius:
            for connector in connector_paths(pose,exit,radius):
                connectors+=1;points=shapely.get_coordinates(connector)
                delta=np.gradient(points,axis=0);angles=np.arctan2(delta[:,1],delta[:,0])
                angles[0],angles[-1]=pose[2],np.arctan2(*exit['heading'][::-1])
                signature=tuple(a+b for a,b in zip(node['signature'],crossing_signature(points,markers)))
                if signature!=wanted or not bodies_fit(points,angles,domain,vehicle):continue
                parts=[points];cursor=index
                while cursor is not None:
                    parts.append(states[cursor]['points']);cursor=states[cursor]['parent']
                xy=np.concatenate(parts[::-1]);xy=xy[np.r_[True,np.linalg.norm(np.diff(xy,axis=0),axis=1)>1e-10]]
                path=shapely.LineString(xy)
                if path.is_simple:
                    return path,dict(reason='sampled_steering_guide',states=expanded,connectors=connectors,
                                     radius_m=radius,cell_m=cell,heading_bins=heading_bins,signature=wanted)
        for turn in (0,-1,1):
            points,angles=steering_points(pose,turn,length,radius)
            if not bodies_fit(points[1:],angles[1:],domain,vehicle):continue
            signature=tuple(a+b for a,b in zip(node['signature'],crossing_signature(points,markers)))
            if any(s<min(0,w)-1 or s>max(0,w)+1 for s,w in zip(signature,wanted)):continue
            successor=np.r_[points[-1],angles[-1]];cost=node['cost']+length;identity=key(successor,signature)
            if cost>=best.get(identity,np.inf)-1e-10:continue
            best[identity]=cost;states.append(dict(pose=successor,cost=cost,signature=signature,parent=index,points=points))
            heapq.heappush(queue,(cost+np.linalg.norm(successor[:2]-exit['position']),len(states)-1))
    return None,dict(reason='steering_state_budget_exhausted' if queue else 'no_sampled_steering_guide',
                     states=expanded,connectors=connectors,radius_m=radius,cell_m=cell,heading_bins=heading_bins,signature=wanted)
