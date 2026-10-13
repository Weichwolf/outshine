"""Extrude logical polygon boundaries, retaining holes, face materials, tags and corner UVs."""

from dataclasses import replace
import math

import numpy as np

from modelling_faces import EXTRUSION_TAGS,Face,face_shape,plane
from modelling_scope import Scope,scalar
from modelling_metadata import inherit


MODES=('face.normal','world.up','world.up.flatTop','vertex.normal')


def active_faces(shape):
    mesh=shape.emit();vertices=mesh['vertices']
    faces=shape.geometry.polygons
    if not faces:
        material=mesh.get('materials',np.ones(len(mesh['faces']),dtype=np.uint16))
        groups=mesh.get('groups',np.zeros(len(mesh['faces']),dtype=np.uint32))
        faces=tuple(Face(tuple(triangle),material=int(value),uvs=(tuple(tuple(pair) for pair in mesh['uv'][triangle]),) if 'uv' in mesh else (),group=int(group))
                    for triangle,value,group in zip(mesh['faces'],material,groups,strict=True))
    if not faces:raise ValueError('Face extrusion requires a nonempty polygon surface')
    used=sorted({index for face in faces for ring in (face.outer,*face.holes) for index in ring})
    mapping={old:new for new,old in enumerate(used)}
    faces=tuple(replace(face,outer=tuple(mapping[i] for i in face.outer),
                        holes=tuple(tuple(mapping[i] for i in ring) for ring in face.holes)) for face in faces)
    return vertices[used],faces


def smooth_normals(vertices,faces,normals):
    result=np.zeros_like(vertices)
    for face,normal in zip(faces,normals,strict=True):
        indices=sorted({index for ring in (face.outer,*face.holes) for index in ring})
        result[indices]+=normal
    lengths=np.linalg.norm(result,axis=1)
    if np.any(lengths<=1e-12):raise ValueError('Adjacent face normals cancel at a vertex')
    return result/lengths[:,None]


def euler(axes):
    horizontal=math.hypot(axes[0,0],axes[1,0])
    y=math.atan2(-axes[2,0],horizontal)
    if horizontal>1e-12:x,z=math.atan2(axes[2,1],axes[2,2]),math.atan2(axes[1,0],axes[0,0])
    else:x,z=math.atan2(-axes[1,2],axes[1,1]),0.
    return tuple(np.rad2deg([x,y,z]))


def aligned_scope(vertices,up,old,negative):
    up=up/np.linalg.norm(up)
    x=old.axes[:,0]-np.dot(old.axes[:,0],up)*up
    if np.linalg.norm(x)<1e-10:x=old.axes[:,1]-np.dot(old.axes[:,1],up)*up
    x/=np.linalg.norm(x);axes=np.column_stack((x,up,np.cross(x,up)))
    coordinates=vertices@axes;low,high=coordinates.min(axis=0),coordinates.max(axis=0)
    size=high-low
    if negative:low[1]=high[1];size[1]=-size[1]
    origin=axes@low
    position=old.pivot_axes.T@(origin-old.pivot_origin)
    angles=euler(old.pivot_axes.T@axes)
    return Scope(tuple(position),angles,tuple(size),old.pivot,old.object_frame)


def displaced(vertices,faces,normals,distance,mode):
    if mode=='vertex.normal':
        vertex=smooth_normals(vertices,faces,normals)
        return vertices+vertex*distance,vertex[faces[0].outer[0]]
    if mode=='world.up':return vertices+np.array([0.,distance,0.]),np.array([0.,1.,0.])
    if mode=='world.up.flatTop':
        extent=float(np.ptp(vertices[:,1]));span=max(abs(distance),extent)
        height=vertices[:,1].max()-span if distance<0 else vertices[:,1].min()+span
        target=vertices.copy();target[:,1]=height
        return target,np.array([0.,1.,0.])
    raise ValueError('Face-normal displacement must be evaluated per face')


def edge_owners(faces):
    result={}
    for index,face in enumerate(faces):
        for ring in (face.outer,*face.holes):
            for a,b in zip(ring,ring[1:]+ring[:1]):
                key=tuple(sorted((a,b)))
                result.setdefault(key,[]).append(index)
    if any(len(owners)>2 for owners in result.values()):raise ValueError('Extrusion input has a nonmanifold polygon edge')
    return result


def cap(face,mapping,reverse,tag):
    rings=tuple(tuple(mapping[index] for index in ring) for ring in (face.outer,*face.holes))
    uv=face.uvs
    if reverse:rings=tuple(ring[::-1] for ring in rings);uv=tuple(ring[::-1] for ring in uv)
    return Face(rings[0],rings[1:],face.material,face.tags|{tag},uv,face.group)


def sides(face,mapping,normals,owners,mode):
    result=[]
    for number,ring in enumerate((face.outer,*face.holes)):
        for corner,(a,b) in enumerate(zip(ring,ring[1:]+ring[:1])):
            shared=owners[tuple(sorted((a,b)))]
            if len(shared)==2 and (mode!='face.normal' or np.allclose(normals[shared[0]],normals[shared[1]],rtol=0.,atol=1e-10)):continue
            uv_a,uv_b=face.uvs[number][corner],face.uvs[number][(corner+1)%len(ring)]
            tag='extrude.side.outer' if number==0 else 'extrude.side.inner'
            result.append(Face((a,b,mapping[b],mapping[a]),material=face.material,tags=face.tags|{tag},uvs=((uv_a,uv_b,uv_b,uv_a),),group=face.group))
    return result


def top_mapping(points,indices,target,normal,known):
    mapping={}
    for index,point in zip(indices,target,strict=True):
        matches=known.setdefault(index,[])
        found=next((value for direction,value in matches if np.allclose(normal,direction,rtol=0.,atol=1e-10)),None)
        if found is None:
            found=len(points);points.append(point.tolist());matches.append((normal,found))
        mapping[index]=found
    return mapping


def extrude(shape,distance,mode='face.normal'):
    distance=scalar(distance)
    if mode not in MODES:raise ValueError('Unknown extrusion type')
    if shape.geometry.edges:
        from modelling_edge_extrude import extrude_edges
        return extrude_edges(shape,distance,mode)
    vertices,faces=active_faces(shape)
    faces=tuple(replace(face,tags=face.tags-EXTRUSION_TAGS.keys()) for face in faces)
    prepared=[plane(vertices,face,True) for face in faces];normals=np.array([item[0] for item in prepared]);faces=tuple(item[3] for item in prepared)
    if distance==0 and mode!='world.up.flatTop':
        return replace(inherit(face_shape(vertices,[replace(face,tags=face.tags|{'extrude.bottom','extrude.top'}) for face in faces],scope=shape.scope,allow_warped=True),shape),comp_info=shape.comp_info)
    owners=edge_owners(faces);output=[];all_points=vertices.tolist();known={}
    target,up=displaced(vertices,faces,normals,distance,mode) if mode!='face.normal' else (None,normals[0])
    if target is not None and np.array_equal(target,vertices):
        return replace(inherit(face_shape(vertices,[replace(face,tags=face.tags|{'extrude.bottom','extrude.top'}) for face in faces],scope=shape.scope,allow_warped=True),shape),comp_info=shape.comp_info)
    for index,face in enumerate(faces):
        selected=sorted({i for ring in (face.outer,*face.holes) for i in ring})
        base=vertices[selected];top=base+normals[index]*distance if mode=='face.normal' else target[selected]
        polarity=np.dot(normals[index],np.mean(top-base,axis=0))
        if abs(polarity)<1e-12:raise ValueError('Tangential or collapsed face extrusion needs a planar sweep contract')
        mapping=top_mapping(all_points,selected,top,normals[index] if mode=='face.normal' else up,known)
        local=[cap(face,{i:i for i in selected},polarity>0,'extrude.bottom'),cap(face,mapping,polarity<0,'extrude.top')]
        walls=sides(face,mapping,normals,owners,mode)
        for wall in walls:
            quad=np.asarray([all_points[i] for i in wall.outer])
            if np.linalg.norm(np.cross(quad[1]-quad[0],quad[2]-quad[0]))+np.linalg.norm(np.cross(quad[2]-quad[0],quad[3]-quad[0]))<1e-12:continue
            local.append(replace(wall,outer=wall.outer[::-1],uvs=tuple(ring[::-1] for ring in wall.uvs)) if polarity<0 else wall)
        output.extend(local)
    points=np.asarray(all_points);scope=aligned_scope(points,up,shape.scope,distance<0)
    return replace(inherit(face_shape(points,output,scope=scope,allow_warped=True),shape),comp_info=shape.comp_info)
