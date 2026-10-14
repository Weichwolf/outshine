"""Metric wall projection for the cached building diagnostic, independent of CGA selectors."""

import numpy as np


def project(mesh,tile_size_m=(2.4,1.2),material=1):
    size=np.asarray(tile_size_m,dtype=float)
    if size.shape!=(2,) or not np.all(np.isfinite(size)) or np.any(size<=0):raise ValueError('Positive tile dimensions required')
    vertices=np.asarray(mesh['vertices']);faces=np.asarray(mesh['faces'])
    rows=np.flatnonzero(mesh['materials']==material);triangles=vertices[faces[rows]]
    u,v=triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]
    normal=np.cross(u,v)
    length=np.linalg.norm(normal,axis=1)
    uncertainty=8*np.finfo(float).eps*np.linalg.norm(abs(u[:,[1,2,0]]*v[:,[2,0,1]])+abs(u[:,[2,0,1]]*v[:,[1,2,0]]),axis=1)
    if np.any(length<=uncertainty):raise ValueError('Degenerate or unresolved wall triangle')
    normal/=length[:,None]
    tangent=np.column_stack((-normal[:,1],normal[:,0],np.zeros(len(rows))))
    horizontal=np.linalg.norm(tangent,axis=1)<1e-8
    tangent[horizontal]=[1.,0.,0.]
    tangent/=np.linalg.norm(tangent,axis=1,keepdims=True)
    bitangent=np.cross(normal,tangent)
    uv=np.stack((np.einsum('tvi,ti->tv',triangles,tangent)/size[0],np.einsum('tvi,ti->tv',triangles,bitangent)/size[1]),axis=-1)
    output={key:value.copy() for key,value in mesh.items()}
    # A corner can need different coordinates on intersecting walls. Retain source triangles and split UV seams.
    output['vertices']=vertices[faces].reshape(-1,3)
    output['faces']=np.arange(faces.size,dtype=faces.dtype).reshape(-1,3)
    output['uv']=np.zeros((faces.size,2));output['uv'].reshape(-1,3,2)[rows]=uv
    for key,value in mesh.items():
        if key not in ('vertices','faces','uv') and len(value)==len(vertices):
            raise NotImplementedError('Diagnostic projection needs explicit vertex-channel remapping: '+key)
    np.testing.assert_array_equal(output['vertices'][output['faces']],vertices[faces])
    return output,dict(projected_triangles=len(rows),source_vertices=len(vertices),output_vertices=len(output['vertices']),
        horizontal_triangles=int(horizontal.sum()),tile_size_m=size.tolist(),
        projection='gravity-aligned planar tangent; horizontal faces use world X; diagnostic only, not CGA setupProjection')
