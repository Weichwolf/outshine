"""Build physical reference scenes from the current cached body and its logical room identities."""

import hashlib
from pathlib import Path

import numpy as np

from interior_light_plan import lights
from opening_probe import scene
from showcase_scene import cached
from showcase_profile import PROFILE


def configuration(label,view,night,output,light_state='all-lit',spp=256):
    root=Path(__file__).resolve().parents[3]/'build/experiments/buildings'
    body=cached(label,root/'opening-inspections',root/'roof-plans');plan=body['plan']
    reference=root/'roof-plans'/('roof-'+label+'-flat-near-scene.json')
    config=scene(reference,output,body['mesh_path'],view if view in ('window','entrance') else 'near',body['slots'])
    if view=='room':
        index=len(plan['rooms'])-1;point=plan['rooms'][index].representative_point();z=float(plan['floor_z_m'])+1.55
        doors=[door for door in plan['doors'] if index in door['rooms']]
        if not doors:raise ValueError('Room inspection requires a connected door')
        target=np.asarray(doors[0]['centre']);config.update(eye=[point.x,point.y,z],target=[*target,z])
    config.update(profile=PROFILE,mesh=str(body['mesh_path'].resolve()),mesh_sha256=hashlib.sha256(body['mesh_path'].read_bytes()).hexdigest(),
                  wall_linear=config['constructive']['wall_linear'],roof_linear=config['constructive']['roof_linear'],
                  glass='dielectric',spp=spp,night=night,room_lights=lights(plan,light_state if night else 'off'))
    provenance=dict(source_id=plan['id'],source_plan_sha256=body['plan_sha256'],cache_source_commit=body['report']['commit'],
                    source_room_ids=plan['storey_room_ids'],geometry_scope='complete cached building, openings, glass and interior geometry; no visibility truncation')
    return config,provenance
