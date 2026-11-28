"""Prepare attribute-preserving surface cuts and distinguish open from closed edge components."""

from dataclasses import dataclass

import numpy as np

from .mesh_fans import connected_fans
from .component_geometry import topology


@dataclass
class CutInput:
    shape: object
    polygons: tuple
    triangle_parts: object
    authored_normals: bool
    properties: object
    triangles: object
    roots: object
    closed: bool
    geometric_normals: object


def closed_components(mesh):
    _,positions=np.unique(mesh['vertices'],axis=0,return_inverse=True);triangles=positions[mesh['faces']]
    parent=np.arange(len(triangles));edges={}
    def root(index):
        while parent[index]!=index:parent[index]=parent[parent[index]];index=parent[index]
        return index
    for index,triangle in enumerate(triangles):
        for a,b in ((triangle[0],triangle[1]),(triangle[1],triangle[2]),(triangle[2],triangle[0])):
            edges.setdefault(tuple(sorted((int(a),int(b)))),[]).append(index)
    for owners in edges.values():
        if len(owners)>2:raise ValueError('Surface cut source has a nonmanifold edge')
        if len(owners)==2:
            a,b=root(owners[0]),root(owners[1]);parent[max(a,b)]=min(a,b)
    components={root(i) for i in range(len(triangles))}
    opened={root(owners[0]) for owners in edges.values() if len(owners)==1}
    kinds={component not in opened for component in components}
    if len(kinds)>1:raise NotImplementedError('Mixed open/closed surface-component cuts remain pending')
    return bool(kinds and True in kinds)


def prepare_source(shape):
    if shape.geometry.edges or shape.geometry.points:raise NotImplementedError('Mixed wire/surface plane cuts remain pending')
    mesh=shape.emit();allowed={'vertices','faces','uv','normals','parts','materials','groups','face_tag_mask'}
    if set(mesh)-allowed:raise ValueError('Plane cut has no inheritance contract for '+str(sorted(set(mesh)-allowed)))
    if not len(mesh['faces']):raise ValueError('Surface cut needs nonempty triangle geometry')
    if not shape.geometry.polygons and 'parts' in mesh:raise NotImplementedError('Raw mesh part provenance without logical faces remains pending')
    polygons,_=topology(shape,mesh);parts=mesh.get('parts',np.arange(len(mesh['faces'])))
    mesh=dict(mesh);mesh.setdefault('uv',np.zeros((len(mesh['vertices']),2)));closed=closed_components(mesh)
    if closed:
        triangles=(mesh['vertices']-mesh['vertices'].mean(axis=0))[mesh['faces']]
        volume=np.einsum('ij,ij->i',triangles[:,0],np.cross(triangles[:,1],triangles[:,2])).sum()/6
        if not np.isfinite(volume) or volume==0:raise ValueError('Closed cut source has no finite nonzero volume')
        if volume<0:mesh['faces']=mesh['faces'][:,::-1]
    properties,triangles,duplicates,targets=connected_fans(mesh,np.arange(len(mesh['faces'])))
    if 'normals' in mesh:properties=np.ascontiguousarray(np.c_[properties,mesh['normals'][mesh['faces']].reshape(-1,3)])
    roots=np.arange(len(properties));roots[duplicates]=targets
    points=properties[roots[triangles],:3]
    normals=np.cross(points[:,1]-points[:,0],points[:,2]-points[:,0]);lengths=np.linalg.norm(normals,axis=1)
    if np.any(lengths<=1e-12):raise ValueError('Surface cut source has a degenerate triangle')
    normals/=lengths[:,None];normals.setflags(write=False)
    return CutInput(shape,polygons,parts,'normals' in mesh,properties,triangles,roots,closed,normals)
