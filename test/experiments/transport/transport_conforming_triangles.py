"""Split triangle edges at existing interface vertices; preserve coordinates and faces."""

import numpy as np
import shapely


def conform_triangles(vertices,indices):
    edges=np.concatenate([indices[:,[0,1]],indices[:,[1,2]],indices[:,[2,0]]])
    edges=np.unique(np.sort(edges,axis=1),axis=0)
    tolerance=64*np.finfo(float).eps*max(1.,float(np.abs(vertices).max(initial=0)))
    tree=shapely.STRtree(shapely.linestrings(vertices[edges]))
    pairs=tree.query(shapely.points(vertices),predicate='dwithin',distance=tolerance)
    splits={}
    for point,edge in pairs.T:
        a,b=edges[edge]
        if point in (a,b):continue
        delta=vertices[b]-vertices[a];length=delta@delta
        if length<=0:continue
        station=float((vertices[point]-vertices[a])@delta/length)
        if 0<station<1:splits.setdefault((int(a),int(b)),[]).append((station,int(point)))
    if not splits:return indices,dict(split_edges=0,added_triangles=0)
    splits={e:[p for _,p in sorted(set(values))] for e,values in splits.items()}
    output=[]
    for original in indices:
        faces=[tuple(map(int,original))]
        for a,b in zip(original,np.roll(original,-1)):
            key=tuple(sorted((int(a),int(b))))
            if key not in splits:continue
            points=splits[key] if a<b else splits[key][::-1]
            owner=next(i for i,f in enumerate(faces) if a in f and b in f)
            opposite=next(p for p in faces[owner] if p not in (a,b))
            chain=[int(a),*points,int(b)]
            faces[owner:owner+1]=[(u,v,opposite) for u,v in zip(chain[:-1],chain[1:])]
        output.extend(faces)
    result=np.array(output,dtype=indices.dtype)
    return result,dict(split_edges=len(splits),added_triangles=len(result)-len(indices))


def verify():
    vertices=np.array([[0.,0.],[4,0],[0,4],[1,0],[3,0],[0,2],[3,-2]])
    indices=np.array([[0,1,2],[3,6,4]])
    result,audit=conform_triangles(vertices,indices)
    assert len(result)==5 and audit['added_triangles']==3
    before=shapely.union_all(shapely.polygons(vertices[indices]));after=shapely.union_all(shapely.polygons(vertices[result]))
    assert before.equals(after)
    edges=np.concatenate([result[:,[0,1]],result[:,[1,2]],result[:,[2,0]]])
    for segment in ((3,4),(0,5),(2,5)):
        assert np.any(np.all(np.sort(edges,axis=1)==segment,axis=1))
    transformed=vertices@np.array([[0.,-1],[1,0]])+np.array([1e6,-7e5])
    moved,_=conform_triangles(transformed,indices);assert np.array_equal(moved,result)
    gap=vertices.copy();gap[[3,4],1]=-.0001
    kept,note=conform_triangles(gap,indices)
    assert note['split_edges']==1 and len(kept)==3
    assert np.array_equal(vertices,np.array([[0.,0.],[4,0],[0,4],[1,0],[3,0],[0,2],[3,-2]]))
    return dict(interface_vertices_inserted=True,multiple_edge_splits=True,area_preserved=True,
                coordinates_unchanged=True,rigid_transform=True,real_gap_not_repaired=True)


if __name__=='__main__':print(verify())
