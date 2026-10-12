"""Measure directed alternatives with bounded shortest-path working memory."""

import numpy as np
from scipy.sparse import csr_matrix
from scipy.sparse.csgraph import dijkstra


def route_detours(node_count,edges,queries,batch_size=16):
    if node_count<0 or batch_size<1:raise ValueError('invalid graph or batch size')
    cheapest={}
    for source,target,length in edges:
        if not 0<=min(source,target)<=max(source,target)<node_count or length<=0 or not np.isfinite(length):
            raise ValueError('invalid directed edge')
        key=source,target;cheapest[key]=min(length,cheapest.get(key,np.inf))
    if any(not 0<=min(a,b)<=max(a,b)<node_count for a,b in queries):raise ValueError('invalid query')
    pairs=np.array(list(cheapest),dtype=int).reshape(-1,2)
    graph=csr_matrix((list(cheapest.values()),(pairs[:,0],pairs[:,1])),shape=(node_count,node_count))
    grouped={}
    for source,target in queries:grouped.setdefault(source,[]).append(target)
    sources=list(grouped);result={}
    for begin in range(0,len(sources),batch_size):
        batch=sources[begin:begin+batch_size]
        distances,parents=dijkstra(graph,directed=True,indices=batch,return_predecessors=True)
        for row,source in enumerate(batch):
            for target in grouped[source]:
                length=float(distances[row,target]);path=[]
                if np.isfinite(length):
                    node=target;path=[node]
                    while node!=source:
                        node=int(parents[row,node]);assert node>=0 and len(path)<=node_count
                        path.append(node)
                    path.reverse()
                result[source,target]=dict(length_m=length if path else None,nodes=path)
    return result


def verify():
    edges=[(0,1,10.),(0,1,2.),(1,2,3.),(0,2,9.),(2,3,1.)]
    queries=[(0,2),(0,3),(3,0),(2,2)]
    a=route_detours(4,edges,queries,1);b=route_detours(4,edges[::-1],queries,3)
    assert a==b and a[0,2]==dict(length_m=5.,nodes=[0,1,2])
    assert a[0,3]['length_m']==6. and a[3,0]['length_m'] is None
    assert a[2,2]==dict(length_m=0.,nodes=[2])
    assert route_detours(0,[],[])=={}
    return dict(shortest_weighted_alternative=True,parallel_edges_use_minimum=True,
        directed_unreachability_preserved=True,batch_size_independent=True,empty_graph=True)


if __name__=='__main__':print(verify())
