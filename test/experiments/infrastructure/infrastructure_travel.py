"""Compile supplied access and one-way semantics independently of physical surfaces."""

from collections import Counter

import numpy as np

from infrastructure_building_clearance import street_kind


MODES = {'motor':1,'cycle':2,'walk':4,'rail':8,'horse':16}
MODE_TAGS = {'motor':'motor_vehicle','cycle':'bicycle','walk':'foot','horse':'horse'}
PUBLIC = {'yes','designated','permissive','official','optional_sidepath'}
RESTRICTED = {'private','destination','permit','customers','delivery','agricultural','forestry'}
DIRECTIONS = {'yes':1,'1':1,'true':1,'-1':-1,'reverse':-1,'no':0,'0':0,'false':0}


def apply_access(public,restricted,bits,key,value,notes):
    value = str(value).lower()
    public,restricted = public&~bits,restricted&~bits
    if value in PUBLIC:
        public |= bits
    elif value in RESTRICTED:
        restricted |= bits
    elif value!='no':
        notes.append(dict(tag=key,value=value,reason='dismount' if value=='dismount' else
                          'use_sidepath' if value=='use_sidepath' else 'unresolved_access',modes=bits))
    return public,restricted


def source_travel(roads,recipes):
    public,restricted = np.zeros((len(roads),2),dtype=np.uint8),np.zeros((len(roads),2),dtype=np.uint8)
    notes = []
    for index,road in enumerate(roads):
        props = road['properties']
        kind = street_kind(props)
        base = recipes.get(kind)
        local = []
        allowed,conditional = sum(MODES[m] for m in base) if base else 0,0
        if base is None:
            local.append(dict(reason='missing_travel_recipe',value=kind,modes=0))
        else:
            for key,bits in (('access',allowed),('vehicle',3),('motor_vehicle',1),('motorcar',1)):
                if key in props:
                    allowed,conditional = apply_access(allowed,conditional,bits,key,props[key],local)
            for mode,key in MODE_TAGS.items():
                if key in props and key!='motor_vehicle':
                    allowed,conditional = apply_access(allowed,conditional,MODES[mode],key,props[key],local)
        forward,reverse = allowed,allowed
        restricted_forward,restricted_reverse = conditional,conditional
        for mode,bit in MODES.items():
            default = props.get('oneway',0) if mode in ('motor','cycle','rail') else 0
            keys = (('oneway:vehicle','oneway:motor_vehicle','oneway:motorcar') if mode=='motor' else
                    ('oneway:vehicle','oneway:bicycle') if mode=='cycle' else (f'oneway:{MODE_TAGS.get(mode,mode)}',))
            value = default
            for key in keys:
                value = props.get(key,value)
            direction = DIRECTIONS.get(str(value).lower())
            if direction is None:
                forward,reverse = forward&~bit,reverse&~bit
                restricted_forward,restricted_reverse = restricted_forward&~bit,restricted_reverse&~bit
                local.append(dict(reason='unresolved_oneway',value=value,modes=bit))
            elif direction==1:
                reverse,restricted_reverse = reverse&~bit,restricted_reverse&~bit
            elif direction==-1:
                forward,restricted_forward = forward&~bit,restricted_forward&~bit
        public[index],restricted[index] = (forward,reverse),(restricted_forward,restricted_reverse)
        notes.extend(dict(source=index,**note) for note in local)
    return public,restricted,notes


def graph_travel(graph,public,restricted):
    owners = graph['owner_sources']
    direction = graph['owner_directions']
    edges = np.repeat(np.arange(len(graph['lines'])),np.diff(graph['owner_offsets']))
    valid = direction!=0
    public_result = np.zeros((len(graph['lines']),2),dtype=np.uint8)
    restricted_result = np.zeros_like(public_result)
    for target,source in ((public_result,public),(restricted_result,restricted)):
        for column in (0,1):
            local = np.where(direction>0,column,1-column)
            np.bitwise_or.at(target[:,column],edges[valid],source[owners[valid],local[valid]])
    return public_result,restricted_result


def compile_travel(roads,graphs,recipes):
    public,restricted,notes = source_travel(roads,recipes)
    arcs = Counter()
    unresolved = 0
    for graph in graphs.values():
        graph['travel_public'],graph['travel_restricted'] = graph_travel(graph,public,restricted)
        unresolved += int(np.count_nonzero(graph['owner_directions']==0))
        for mode,bit in MODES.items():
            arcs[mode] += int(np.count_nonzero(graph['travel_public']&bit))
    return dict(public_directed_arcs=dict(arcs),source_notes=notes,
                source_note_reasons=dict(Counter(n['reason'] for n in notes)),
                unresolved_owner_directions=unresolved,
                scope='Supplied access/direction and plausible class defaults; lanes, turn restrictions and swept-body routes remain open.')
