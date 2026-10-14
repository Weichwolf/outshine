"""Small roof envelopes with one shared constructive room/opening plan."""

import time
from dataclasses import replace
from types import MappingProxyType

import numpy as np

from boolean_building_plan import sources
from building_vertical import build
from structural_shell_geometry import assembly
from tile_room_details import numeric_properties
from verify_structural_shell import access
from outshine_ref.buildings.modelling import heightfield_envelope,PrismVolume,subtract_prisms
from outshine_ref.buildings.modelling.prism_union import frozen


def plan_for(row,choice):
    sample=dict(id=row['id'],polygon=row['polygon'],properties=numeric_properties(row))
    plan=build(sample,body_top_m=choice.eave_m)
    if plan.get('vertical_status')!='connected-storeys':raise ValueError('No complete room/storey plan below the source roof')
    plan['roof_choice']=choice.__dict__
    plan['scope']='Connected generic rooms below the same source-stable roof; unfitted roof space and native controller remain unproved'
    return plan


def structure(plan,envelope,wall_material):
    prisms=sources(plan)
    cutters=[PrismVolume(plan['id']+'/'+name,prism.polygon,prism.low,prism.high,1)
             for name,prism in prisms.items() if name!='outer']
    shell=subtract_prisms(plan['id']+'/roof-envelope',envelope,cutters)
    faces=shell.mesh['faces'];points=shell.mesh['vertices'][faces]
    normals=np.cross(points[:,1]-points[:,0],points[:,2]-points[:,0]);normals/=np.linalg.norm(normals,axis=1,keepdims=True)
    materials=shell.mesh['materials'].copy()
    for index,name in enumerate(shell.source_ids):
        if name.endswith('/rooms'):
            selected=shell.source_indices==index
            materials[selected&(normals[:,2]>.5)]=0
            materials[selected&(normals[:,2]<-.5)]=2
    materials[materials==1]=wall_material
    return replace(shell,mesh=MappingProxyType(dict(shell.mesh,materials=frozen(materials))))


def complete(row,parameters,material,choice,top):
    began=time.perf_counter();plan=plan_for(row,choice);plan_ms=(time.perf_counter()-began)*1000
    envelope=heightfield_envelope(top,parameters['minHeight'],wall_material=material,base_material=0)
    began=time.perf_counter();shell=structure(plan,envelope,material);shell_ms=(time.perf_counter()-began)*1000
    began=time.perf_counter();mesh,collision=assembly(plan,shell,frames=True);assembly_ms=(time.perf_counter()-began)*1000
    proof=access(plan,collision)
    points=mesh['vertices'][mesh['faces']];normals=np.cross(points[:,1]-points[:,0],points[:,2]-points[:,0])
    norms=np.linalg.norm(normals,axis=1)
    if np.any(norms==0):raise ValueError('Pitched room assembly has unresolved triangles')
    normals/=norms[:,None]
    uv=np.zeros((len(mesh['vertices']),2));uv[:len(shell.mesh['vertices'])]=shell.mesh['uv']
    result=dict(vertices=points.reshape(-1,3),faces=np.arange(points.size//3,dtype=np.uint32).reshape(-1,3),
        normals=np.repeat(normals,3,axis=0),uv=uv[mesh['faces']].reshape(-1,2),materials=mesh['materials'].astype(np.uint16))
    info=dict(form='Complete generic rooms, doors, windows and source roof',storeys=plan['storeys'],rooms=len(plan['rooms'])*plan['storeys'],
        room_ids=plan['storey_room_ids'],access_mode=plan['circulation_status'],routing_attempts=len(plan['circulation_alternatives']),
        plan_ms=plan_ms,shell_ms=shell_ms,assembly_ms=assembly_ms,access=proof,
        functional_status='Sampled actual roof/room/opening access; generic programme, external contact and native controller unproved')
    return result,info,plan,shell
