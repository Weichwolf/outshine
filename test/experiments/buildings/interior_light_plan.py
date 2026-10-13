"""Derive camera-independent ceiling fixtures from existing room and storey identities."""

import hashlib
import numpy as np


def lights(plan,state='all-lit'):
    if state not in ('all-lit','mixed','off'):raise ValueError('Unknown diagnostic room-light state')
    result=[]
    for floor,identifiers in enumerate(plan['storey_room_ids']):
        ceiling=float(plan['ceiling_z_m'])+floor*(plan['body_top_m']-plan['base_z_m'])/plan['storeys']
        for index,(room,identifier) in enumerate(zip(plan['rooms'],identifiers,strict=True)):
            centre=room.representative_point();clearance=room.boundary.distance(centre)
            size=min(.8,float(clearance*np.sqrt(2.)*.8))
            enabled=state=='all-lit' or state=='mixed' and int(hashlib.sha256(identifier.encode()).hexdigest()[:8],16)%3!=0
            reserved=plan['room_kinds'][index]=='service-reservation'
            result.append(dict(room_id=identifier,storey=floor,position=[centre.x,centre.y,ceiling-.04],size=[size,size],
                enabled=enabled and size>.02 and not reserved,radiance=[18.,12.,6.],
                omitted_reason='service reservation remains unlit' if reserved else 'insufficient fixture clearance' if size<=.02 else '',
                origin='procedural ceiling fixture inside cached room; shared across cameras'))
    return result
