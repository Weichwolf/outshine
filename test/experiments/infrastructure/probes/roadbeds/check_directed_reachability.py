"""Compare batched component reachability with independent source searches."""

from collections import deque
from pathlib import Path
import sys

import numpy as np

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from infrastructure_route_connectivity import compare_connectivity,verify_connectivity


def reachable(count,edges,source):
    outgoing=[[] for _ in range(count)]
    for a,b in edges:outgoing[a].append(b)
    seen={source};pending=deque([source])
    while pending:
        for other in outgoing[pending.popleft()]:
            if other not in seen:seen.add(other);pending.append(other)
    return seen


def check(count,edges,queries):
    result=compare_connectivity(count,edges|queries,edges)
    expected={source:reachable(count,edges,source) for source,_ in queries}
    for row in result['missing_edges']:
        assert (row['reason']=='already_reachable')==(row['target'] in expected[row['source']]),row


rng=np.random.default_rng(2343)
for count in range(1,41):
    for _ in range(5):
        edges={tuple(map(int,p)) for p in rng.integers(0,count,size=(count*2,2))}
        queries={tuple(map(int,p)) for p in rng.integers(0,count,size=(count*3,2))}
        check(count,edges,queries)
check(140,{(i,i+1) for i in range(139)},{(0,i) for i in range(2,140)}|{(i,0) for i in range(1,140)})
print(dict(random_graphs=200,multiple_target_batches_checked=True,**verify_connectivity()))
