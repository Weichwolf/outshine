"""Split triangle edges at existing interface vertices; preserve coordinates and faces."""


import numpy as np
import shapely

from outshine_ref.geometry import conform_triangles


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
