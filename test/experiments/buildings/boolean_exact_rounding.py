"""Contract only output edges whose endpoints round to identical Float64 positions."""

import numpy as np


def contract(roots,supports,native,nodes):
    parents={int(root):int(root) for root in np.unique(roots)}

    def find(root):
        while parents[root]!=root:
            parents[root]=parents[parents[root]];root=parents[root]
        return root

    for triangle in roots:
        for a,b in zip(triangle,np.roll(triangle,-1),strict=True):
            first,last=int(a),int(b)
            if not np.array_equal(nodes.coordinates[first],nodes.coordinates[last]):continue
            a,b=sorted((find(first),find(last)));parents[b]=a
    mapped=np.asarray([find(int(root)) for root in roots.ravel()],dtype=np.int64).reshape(-1,3)
    keep=np.all(mapped!=np.roll(mapped,1,axis=1),axis=1)
    return mapped[keep],tuple(s for s,k in zip(supports,keep,strict=True) if k),np.asarray(native,dtype=np.int32)[keep]
