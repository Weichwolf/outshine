"""Rank shared port headings before constructing and publishing coupled roadbeds."""

import numpy as np

from infrastructure_curvature_paths import fit_curvature_paths
from infrastructure_cut_setbacks import fork_cut_plan
from infrastructure_junction_surfaces import motor_surface
from infrastructure_shared_frames import frame_key,heading_choices


def joint_heading_trials(network,module,request,maximum_candidates=64):
    poses,_=network.poses(module,{})
    by_request={frame_key(p)[:3]:p for p in poses}
    ports=[by_request[key] for key in request]
    owners=set.union(*(network.owners[frame_key(p)] for p in ports))
    floors={owner:motor_surface(network.shape(owner),network.graph,network.tree,network.mesh,network.vehicle,
                                network.curbs['corner_radius_m'],network.curbs['arc_error_m']) for owner in owners}
    options=[heading_choices(p,network.anchors[frame_key(p)],[floors[k] for k in network.owners[frame_key(p)]],
                             network.vehicle,network.curbs['arc_error_m']) for p in ports]
    if not all(options):return
    choices=[(a,b) for a in options[0] for b in options[1]]
    starts=np.repeat(ports[0]['position'][None,:],len(choices),axis=0)
    ends=np.repeat(ports[1]['position'][None,:],len(choices),axis=0)
    angles=np.array([[np.arctan2(a[0][1],a[0][0]),np.arctan2(b[0][1],b[0][0])] for a,b in choices])
    radius=network.vehicle['minimumRearAxleRadiusM']
    fits=fit_curvature_paths(starts,ends,angles[:,0],angles[:,1],minimum_radius=radius)
    valid=fits['valid']&(fits['maximum_curvature_per_m']*radius<=1+1e-12)
    ranked=sorted(np.flatnonzero(valid),key=lambda index:sum(p[1] for p in choices[index]))
    for index in ranked[:maximum_candidates]:
        selected=choices[index]
        if all(np.linalg.norm(p['heading']-h[0])<1e-10 for p,h in zip(ports,selected)):continue
        trial=fork_cut_plan(network,module,set())
        sections=set()
        for pose,(heading,_) in zip(ports,selected):
            trial.frames[frame_key(pose)]=dict(position=pose['position'].copy(),heading=heading.copy())
            sections.add(pose['section'])
        yield trial,sections,owners,sum(h[1] for h in selected)
