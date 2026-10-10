"""Inspect each connected shared interface as an actual transverse port section."""

import time

import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components


def port_sections(product,tolerance=1e-5):
    began = time.perf_counter()
    edges,parts = product['port_edges'],np.sort(product['port_parts'],axis=1)
    if not len(edges):
        return dict(sections=0,straight_sections=0,invalid_sections=0),np.empty((0,2))
    keys = np.c_[np.repeat(parts,2,axis=0),edges.ravel()]
    nodes,labels = np.unique(keys,axis=0,return_inverse=True)
    pair = labels.reshape(-1,2)
    links = coo_matrix((np.ones(len(pair)),pair.T),shape=(len(nodes),len(nodes))).tocsr()
    count,components = connected_components(links,directed=False)
    degrees = np.bincount(labels,minlength=len(nodes))
    endpoints = np.flatnonzero(degrees==1)
    endpoint_count = np.bincount(components[endpoints],minlength=count)
    first,last = np.full(count,len(nodes)),np.full(count,-1)
    np.minimum.at(first,components[endpoints],endpoints)
    np.maximum.at(last,components[endpoints],endpoints)
    open_path = endpoint_count==2
    xy = product['vertices'][nodes[:,2]]
    origin,direction = np.zeros((count,2)),np.zeros((count,2))
    origin[open_path] = xy[first[open_path]]
    direction[open_path] = xy[last[open_path]]-origin[open_path]
    length = np.linalg.norm(direction,axis=1)
    direction /= np.where(length>0,length,1)[:,None]
    offset = xy-origin[components]
    normal_distance = np.abs(offset[:,0]*direction[components,1]-offset[:,1]*direction[components,0])
    deviation,degree = np.zeros(count),np.zeros(count,dtype=int)
    np.maximum.at(deviation,components,normal_distance)
    np.maximum.at(degree,components,degrees)
    roles = product['part_roles'][nodes[:,:2]]
    wrong_role = np.zeros(count,dtype=bool)
    np.logical_or.at(wrong_role,components,roles[:,0]==roles[:,1])
    valid = open_path&(degree<=2)&(length>0)&(deviation<=tolerance)&~wrong_role
    centres = np.zeros((count,2))
    np.add.at(centres,components,xy)
    centres /= np.bincount(components,minlength=count)[:,None]
    return dict(sections=count,straight_sections=int(valid.sum()),invalid_sections=int((~valid).sum()),
                closed_or_branching_sections=int((~open_path|(degree>2)).sum()),
                curved_open_sections=int((open_path&(deviation>tolerance)).sum()),
                wrong_module_roles=int(wrong_role.sum()),maximum_line_deviation_m=float(deviation[open_path].max(initial=0)),
                straight_tolerance_m=tolerance,audit_ms=(time.perf_counter()-began)*1000),centres[~valid]
