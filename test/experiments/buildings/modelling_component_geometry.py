"""Take logical face subsets from existing triangles and fit their component frames."""

from dataclasses import replace

import numpy as np

from modelling_extrude import euler
from modelling_faces import Face
from modelling_scope import Frame,Scope
from modelling_shape import Shape


def face_normals(vertices,faces):
    normals=[]
    for face in faces:
        points=vertices[list(face.outer)];points=points-points[0]
        vector=np.cross(points,np.roll(points,-1,axis=0)).sum(axis=0);length=np.linalg.norm(vector)
        if length<=1e-12:raise ValueError('A component face needs a nonzero geometric normal')
        normals.append(vector/length)
    return np.asarray(normals)


def topology(shape,mesh):
    faces=shape.geometry.polygons
    if faces:
        parts=mesh.get('parts')
        if parts is None or parts.shape!=(len(mesh['faces']),) or parts.dtype.kind not in 'iu' or (len(parts) and (parts.min()<0 or parts.max()>=len(faces))):
            raise ValueError('Logical component faces require valid native triangle-to-face indices')
    else:
        material=mesh.get('materials',np.ones(len(mesh['faces']),dtype=np.uint16))
        faces=tuple(Face(tuple(triangle),material=int(value),uvs=(tuple(tuple(pair) for pair in mesh['uv'][triangle]),) if 'uv' in mesh else ())
                    for triangle,value in zip(mesh['faces'],material,strict=True))
        parts=np.arange(len(faces))
    rows=[[] for _ in faces]
    for index,part in enumerate(parts):rows[int(part)].append(index)
    return tuple(faces),tuple(tuple(row) for row in rows)


def subset(mesh,faces,triangle_rows,indices):
    rows=np.array([row for index in indices for row in triangle_rows[index]],dtype=np.int64)
    triangles=mesh['faces'][rows]
    logical=[vertex for index in indices for ring in (faces[index].outer,*faces[index].holes) for vertex in ring]
    used=np.unique(np.r_[triangles.ravel(),logical]).astype(np.int64)
    mapping={int(old):int(new) for new,old in enumerate(used)}
    native=dict(vertices=mesh['vertices'][used],faces=np.searchsorted(used,triangles).astype(np.int32))
    for name,array in mesh.items():
        if name in ('vertices','faces','parts'):continue
        if name in ('uv','normals'):native[name]=array[used]
        elif name in ('materials','face_tag_mask'):native[name]=array[rows]
        else:raise ValueError('Component selection has no field contract for '+name)
    native['parts']=np.concatenate([np.full(len(triangle_rows[index]),number,dtype=np.int32) for number,index in enumerate(indices)])
    polygons=tuple(replace(faces[index],outer=tuple(mapping[i] for i in faces[index].outer),
                    holes=tuple(tuple(mapping[i] for i in ring) for ring in faces[index].holes)) for index in indices)
    return native,polygons


def fit_scope(points,axes,origin,old,pivot=None,signed=False):
    local=(points-origin)@axes;low,high=local.min(axis=0),local.max(axis=0)
    flat=(np.abs(low)<1e-8)&(np.abs(high)<1e-8);low[flat]=0.;high[flat]=0.
    size=high-low;collapsed=size<1e-8;low[collapsed]=(low[collapsed]+high[collapsed])/2;size[collapsed]=0.
    if signed:
        negative=np.asarray(old.size)<0;low[negative]=high[negative];size[negative]*=-1
    if pivot is None:
        position=old.pivot_axes.T@(origin+axes@low-old.pivot_origin)
        return Scope(tuple(position),old.orientation,tuple(size),old.pivot,old.object_frame)
    return Scope(tuple(low),(0.,0.,0.),tuple(size),pivot,old.object_frame)


def component_scope(mesh,face,normal,old,alignment):
    if alignment=='noAlign':return fit_scope(mesh['vertices'],old.axes,old.origin,old,signed=True)
    first,second=face.outer[:2];origin=mesh['vertices'][first]
    edge=mesh['vertices'][second]-origin;x=edge-normal*np.dot(edge,normal);length=np.linalg.norm(x)
    if length<=1e-12:raise ValueError('Face component alignment requires a usable first edge')
    x/=length;axes=np.column_stack((x,np.cross(normal,x),normal))
    relative=old.object_frame.axes.T@axes
    pivot=Frame(tuple(old.object_frame.axes.T@(origin-old.object_frame.position)),euler(relative))
    return fit_scope(mesh['vertices'],axes,origin,old,pivot)


def component_shape(surface,indices,alignment,info):
    mesh,polygons=subset(surface.mesh,surface.faces,surface.triangle_rows,indices)
    scope=component_scope(mesh,polygons[0],surface.normals[indices[0]],surface.source.scope,alignment)
    return replace(Shape.from_mesh(mesh,scope,polygons),comp_info=info)
