"""Canonical vertex identities within an explicit numerical displacement bound."""

import numpy as np
from scipy.spatial import cKDTree
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components


def welded_vertices(vertices,tolerance):
    pairs = cKDTree(vertices).query_pairs(tolerance,output_type='ndarray')
    if not len(pairs):
        return vertices,np.arange(len(vertices)),0.
    links = coo_matrix((np.ones(len(pairs)),pairs.T),shape=(len(vertices),len(vertices))).tocsr()
    _,labels = connected_components(links,directed=False)
    first = np.full(labels.max()+1,len(vertices),dtype=int)
    np.minimum.at(first,labels,np.arange(len(vertices)))
    canonical = vertices[first]
    displacement = np.linalg.norm(vertices-canonical[labels],axis=1).max(initial=0)
    if displacement>tolerance:
        raise ValueError('vertex welding exceeds its numeric tolerance')
    return canonical,labels,float(displacement)
