"""Compare requested and proved route graphs in linear sparse-graph passes."""

from collections import Counter,defaultdict,deque

import numpy as np
from scipy.sparse import csr_matrix
from scipy.sparse.csgraph import connected_components

from transport_shared_frames import frame_key


def component_splits(before,after):
    groups = defaultdict(set)
    for a,b in zip(before,after):
        groups[int(a)].add(int(b))
    sizes = Counter(map(int,before))
    return [dict(requested_component=a,nodes=sizes[a],proved_components=len(parts))
            for a,parts in groups.items() if len(parts)>1]


def reachable_component_pairs(labels,edges,queries):
    count=int(max(labels,default=-1))+1
    successors=[set() for _ in range(count)]
    indegree=[0]*count
    for a,b in edges:
        a,b=int(labels[a]),int(labels[b])
        if a!=b and b not in successors[a]:
            successors[a].add(b);indegree[b]+=1
    queue=deque(i for i,n in enumerate(indegree) if not n)
    order=[]
    while queue:
        node=queue.popleft();order.append(node)
        for other in successors[node]:
            indegree[other]-=1
            if not indegree[other]:queue.append(other)
    assert len(order)==count,'strong-component graph must be acyclic'
    targets=defaultdict(list)
    for a,b in queries:targets[int(labels[b])].append((a,b))
    targets=list(targets.items());reachable=set()
    for begin in range(0,len(targets),64):
        batch=targets[begin:begin+64];masks=[0]*count
        for bit,(target,_) in enumerate(batch):masks[target]=1<<bit
        for node in reversed(order):
            for other in successors[node]:masks[node]|=masks[other]
        for bit,(_,pairs) in enumerate(batch):
            reachable.update((a,b) for a,b in pairs if masks[int(labels[a])]&(1<<bit))
    return reachable


def compare_connectivity(node_count,requested,proved):
    requested = set(map(tuple,requested))
    proved = set(map(tuple,proved))
    if node_count<0 or not proved<=requested or any(min(pair)<0 or max(pair)>=node_count for pair in requested):
        raise ValueError('route graph contains invalid nodes or unrequested edges')
    labels = []
    for edges in (requested,proved):
        pairs = np.array(list(edges),dtype=int).reshape(-1,2)
        graph = csr_matrix((np.ones(len(pairs),dtype=bool),(pairs[:,0],pairs[:,1])),shape=(node_count,node_count))
        labels.append({mode:connected_components(graph,directed=True,connection=mode)
                       for mode in ('weak','strong')})
    before,after = labels
    missing = []
    alternatives=reachable_component_pairs(after['strong'][1],proved,requested-proved)
    for a,b in sorted(requested-proved):
        reason = ('already_reachable' if (a,b) in alternatives else
                  'disconnected' if after['weak'][1][a]!=after['weak'][1][b] else 'cross_directed_components')
        missing.append(dict(source=a,target=b,reason=reason))
    return dict(nodes=node_count,requested_edges=len(requested),proved_edges=len(proved),
                requested_weak_components=before['weak'][0],proved_weak_components=after['weak'][0],
                requested_strong_components=before['strong'][0],proved_strong_components=after['strong'][0],
                weak_splits=component_splits(before['weak'][1],after['weak'][1]),
                strong_splits=component_splits(before['strong'][1],after['strong'][1]),
                missing_edge_reasons=dict(Counter(r['reason'] for r in missing)),missing_edges=missing)


def network_connectivity(network,report):
    nodes,requested,proved,origins = {},[],[],defaultdict(list)
    for row in report['modules']:
        poses,_ = network.poses(row['module'],{})
        ids = []
        for pose in poses:
            key = frame_key(pose)
            if key not in nodes:
                nodes[key] = len(nodes)
            ids.append(nodes[key])
        for turn in row['turns']:
            pair = ids[turn['entry']],ids[turn['exit']]
            requested.append(pair)
            origins[pair].append(dict(module=row['module'],entry=turn['entry'],exit=turn['exit']))
            if turn['accepted']:
                proved.append(pair)
    result = compare_connectivity(len(nodes),requested,proved)
    for row in result['missing_edges']:
        row['routes'] = origins[row['source'],row['target']]
    return dict(place=report['place'],**result)


def verify_connectivity():
    chain = compare_connectivity(3,[(0,1),(1,2)],[(0,1)])
    assert len(chain['weak_splits'])==1 and chain['missing_edge_reasons']=={'disconnected':1}
    cycle = compare_connectivity(3,[(0,1),(1,2),(2,0)],[(0,1),(1,2)])
    assert not cycle['weak_splits'] and cycle['strong_splits']==[dict(requested_component=0,nodes=3,proved_components=3)]
    assert cycle['missing_edge_reasons']=={'cross_directed_components':1}
    alternative = compare_connectivity(3,[(0,1),(1,2),(2,0),(0,2)],[(0,1),(1,2),(2,0)])
    assert not alternative['strong_splits'] and alternative['missing_edge_reasons']=={'already_reachable':1}
    oneway = compare_connectivity(3,[(0,1),(1,2),(0,2),(2,0)],[(0,1),(1,2)])
    assert oneway['missing_edge_reasons']=={'already_reachable':1,'cross_directed_components':1}
    duplicate = compare_connectivity(2,[(0,1),(0,1)],[(0,1)])
    assert not duplicate['missing_edges'] and duplicate['requested_edges']==1
    empty = compare_connectivity(0,[],[])
    assert empty['nodes']==0 and not empty['strong_splits'] and not empty['weak_splits']
    return dict(disconnected_branch_detected=True,broken_cycle_detected=True,
                alternative_route_recognized=True,oneway_alternative_checked=True,duplicate_edges_collapsed=True,empty_network_checked=True)
