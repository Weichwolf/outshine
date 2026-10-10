"""Keep junction connections within the source-permitted directed local network."""

import heapq

import numpy as np
import shapely
from shapely.ops import substring

from infrastructure_travel import MODES


def shortest_node_routes(adjacency,start):
    distance,parents = {start:0.},{start:None}
    pending = [(0.,start)]
    while pending:
        length,source = heapq.heappop(pending)
        if length!=distance[source]:
            continue
        for target,edge,cost in adjacency.get(source,()):
            candidate = length+cost
            if candidate<distance.get(target,np.inf):
                distance[target] = candidate
                parents[target] = source,edge
                heapq.heappush(pending,(candidate,target))
    return parents


def source_route(graph,entry,exit,parents,area):
    if entry['edge']==exit['edge'] and entry['direction']==exit['direction']:
        if entry['direction']*(exit['station']-entry['station'])>1e-6:
            part = substring(graph['lines'][entry['edge']],entry['station'],exit['station'])
            if shapely.covers(area,part):
                return part
    edge = graph['edges'][exit['edge']]
    target = int(edge[0] if exit['direction']==1 else edge[1])
    if target not in parents:
        return None
    middle = []
    while parents[target] is not None:
        source,index = parents[target]
        xy = shapely.get_coordinates(graph['lines'][index])
        middle.append(xy if graph['edges'][index,0]==source else xy[::-1])
        target = source
    incoming,outgoing = graph['lines'][entry['edge']],graph['lines'][exit['edge']]
    before = substring(incoming,entry['station'],incoming.length if entry['direction']==1 else 0)
    after = substring(outgoing,0 if exit['direction']==1 else outgoing.length,exit['station'])
    points = np.vstack((shapely.get_coordinates(before),*middle[::-1],shapely.get_coordinates(after)))
    points = points[np.r_[True,np.linalg.norm(np.diff(points,axis=0),axis=1)>1e-10]]
    if len(points)<2:
        return None
    path = shapely.LineString(points)
    return path if shapely.covers(area,path) else None


def directed_junction_routes(graph,poses,shape,tree,tolerance=1e-6):
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
            adjacency.setdefault(source,[]).append((target,int(edge),graph['lines'][edge].length))
    routes,counts,cache = {},dict(no_directed_connection=0,u_turns=0),{}
    for i,entry in enumerate(poses):
        if not entry['incoming']:
            continue
        edge = graph['edges'][entry['edge']]
        node = int(edge[1] if entry['direction']==1 else edge[0])
        if node not in cache:
            cache[node] = shortest_node_routes(adjacency,node) if node in inside else {}
        for j,exit in enumerate(poses):
            if exit['incoming']:
                continue
            if entry['section']==exit['section']:
                counts['u_turns'] += 1
                continue
            path = source_route(graph,entry,exit,cache[node],area)
            if path is not None:
                routes[i,j] = path
            else:
                counts['no_directed_connection'] += 1
    return routes,counts
