"""Keep junction connections within the source-permitted directed local network."""

from collections import deque

import numpy as np
import shapely
from shapely.ops import substring

from infrastructure_travel import MODES


def reachable_nodes(adjacency,start):
    found = {start}
    pending = deque([start])
    while pending:
        for target in adjacency.get(pending.popleft(),()):
            if target not in found:
                found.add(target)
                pending.append(target)
    return found


def allowed_connections(graph,poses,shape,tree,tolerance=1e-6):
    local = tree.query(shape,predicate='intersects')
    permitted = graph['travel_public'][local]&MODES['motor']!=0
    local = local[np.any(permitted,axis=1)]
    vertices = np.unique(graph['edges'][local])
    area = shapely.buffer(shape,tolerance)
    shapely.prepare(area)
    inside = set(vertices[shapely.covers(area,shapely.points(graph['vertices'][vertices]))].tolist())
    adjacency = {}
    for edge in local:
        a,b = map(int,graph['edges'][edge])
        if a not in inside or b not in inside or not shapely.covers(area,graph['lines'][edge]):
            continue
        flags = graph['travel_public'][edge]&MODES['motor']!=0
        for column in np.flatnonzero(flags):
            source,target = (a,b) if column==0 else (b,a)
            adjacency.setdefault(source,[]).append(target)
    connections,counts = [],dict(no_directed_connection=0,u_turns=0)
    for i,entry in enumerate(poses):
        if not entry['incoming']:
            continue
        edge = graph['edges'][entry['edge']]
        node = int(edge[1] if entry['direction']==1 else edge[0])
        reachable = reachable_nodes(adjacency,node) if node in inside else set()
        for j,exit in enumerate(poses):
            if exit['incoming']:
                continue
            if entry['section']==exit['section']:
                counts['u_turns'] += 1
                continue
            edge = graph['edges'][exit['edge']]
            target = int(edge[0] if exit['direction']==1 else edge[1])
            direct = False
            if entry['edge']==exit['edge'] and entry['direction']==exit['direction']:
                if entry['direction']*(exit['station']-entry['station'])>tolerance:
                    part = substring(graph['lines'][entry['edge']],entry['station'],exit['station'])
                    direct = bool(shapely.covers(area,part))
            if target in reachable or direct:
                connections.append((i,j))
            else:
                counts['no_directed_connection'] += 1
    return connections,counts
