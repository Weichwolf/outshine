"""Both operand tags and intersection edges on geometrically partitioned CSG regions."""

from dataclasses import replace

import numpy as np

from boolean_attributes_probe import bind_attributes,immutable,BOOL_AUTO_TAGS


def attributes(overlay):
    bound=bind_attributes(overlay.product);tags=[]
    for base,supports in zip(bound.tags,overlay.supports,strict=True):
        captured=base
        for owner,triangle in supports:
            source=overlay.product.sources[owner];part=int(source.mesh['parts'][triangle])
            captured=captured|(source.shape.geometry.polygons[part].tags-BOOL_AUTO_TAGS)|{'bool.A' if owner==0 else 'bool.B'}
        tags.append(captured)
    return replace(bound,tags=tuple(tags))


def cut_edges(overlay):
    product=overlay.product;faces=product.roots[product.mesh['faces']];edges={}
    masks=[sum(1<<owner for owner,_ in supports) for supports in overlay.supports]
    for number,triangle in enumerate(faces):
        for a,b in zip(triangle,np.roll(triangle,-1)):
            edges.setdefault(tuple(sorted((int(a),int(b)))),[]).append(number)
    result=[]
    for edge,owners in edges.items():
        if len(owners)!=2:raise ValueError('Semantic intersection edge has invalid closed adjacency')
        a,b=owners
        if masks[a]!=masks[b] or product.operands[a]!=product.operands[b]:result.append(edge)
    return immutable(np.asarray(result,dtype=np.int64).reshape(-1,2),np.int64)
