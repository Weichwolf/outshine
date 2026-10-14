"""Close inset roof rims using the exact native boundary-chart corners."""

from dataclasses import replace

import numpy as np

from .uv import name


def close_rim(builder, normal, heights, face):
    mesh, roots, bottoms = builder.bottom_faces()
    holes = {tuple(sorted((int(roots[a]),int(roots[b])))) for bottom in bottoms for ring in bottom.holes
             for a,b in zip(ring,ring[1:]+ring[:1])}
    edges = {}
    for triangle in mesh['faces']:
        for a,b in zip(triangle,np.roll(triangle,-1)):
            key = tuple(sorted((int(roots[a]),int(roots[b]))))
            edges.setdefault(key,[]).append((int(a),int(b)))
    for key, entries in edges.items():
        if len(entries)!=1:continue
        a,b = entries[0]
        values = np.c_[mesh['vertices'][[b,a]],*[mesh[name(channel)][[b,a]] for channel in builder.channels]]
        identities = [int(roots[b]),int(roots[a])]
        rise = np.asarray([heights[root] for root in identities])
        if not np.any(rise):continue
        upper = values.copy();upper[:,:3]+=rise[:,None]*normal
        top = [root if height==0 else builder.root(('top',root)) for root,height in zip(identities,rise,strict=True)]
        raw = identities+top[::-1]
        keep = [i for i in range(4) if raw[i]!=raw[i-1]]
        tag = 'roof.side.inner' if key in holes else 'roof.side.outer'
        builder.polygon(np.r_[values,upper[::-1]][keep],[raw[i] for i in keep],('side',*key),
                        replace(face,tags=face.tags|{tag}))
