"""Recover logical polygon boundaries from retained triangle/vertex relationships."""

from dataclasses import replace

import numpy as np
import shapely

from .faces import Face
from .vector import cross3


def vertex_roots(encoded,count):
    roots=np.arange(count)
    def root(index):
        while roots[index]!=index:
            roots[index]=roots[roots[index]];index=roots[index]
        return index
    for a,b in zip(encoded.merge_from_vert,encoded.merge_to_vert,strict=True):
        a,b=root(int(a)),root(int(b));roots[max(a,b)]=min(a,b)
    return np.asarray([root(i) for i in range(count)])


def boundary_loops(triangles,roots):
    boundary={}
    for triangle in triangles:
        for a,b in ((triangle[0],triangle[1]),(triangle[1],triangle[2]),(triangle[2],triangle[0])):
            key=(int(roots[a]),int(roots[b]));reverse=key[::-1]
            if reverse in boundary:del boundary[reverse]
            elif key in boundary:raise ValueError('Logical cut face has duplicate directed boundaries')
            else:boundary[key]=(int(a),int(b))
    outgoing={}
    for a,b in boundary:
        if a in outgoing:raise ValueError('Logical cut boundary branches at a vertex')
        outgoing[a]=b
    loops=[]
    while outgoing:
        first=next(iter(outgoing));current=first;loop=[]
        while True:
            if current not in outgoing:raise ValueError('Logical cut face has an open boundary')
            other=outgoing.pop(current);loop.append(boundary[(current,other)][0]);current=other
            if current==first:break
        if len(loop)<3:raise ValueError('Logical cut face has a degenerate boundary')
        loops.append(tuple(loop))
    return tuple(loops)


def polygon_faces(vertices,uv,triangles,roots,template):
    loops=boundary_loops(triangles,roots)
    if not loops:return ()
    points=vertices[triangles];normal=np.cross(points[:,1]-points[:,0],points[:,2]-points[:,0]).sum(axis=0)
    length=np.linalg.norm(normal)
    if length<=1e-12:raise ValueError('Logical cut face has no stable area normal')
    normal/=length;axis=np.eye(3)[np.argmin(abs(normal))];x=cross3(axis,normal);x/=np.linalg.norm(x)
    basis=np.array([x,cross3(normal,x)]);origin=vertices[loops[0][0]]
    projected=[(vertices[list(loop)]-origin)@basis.T for loop in loops]
    rings=[shapely.Polygon(points) for points in projected]
    if any(not polygon.is_valid or polygon.area<=1e-12 for polygon in rings):raise ValueError('Invalid projected cut-face boundary')
    areas=[.5*np.sum(points[:,0]*np.roll(points[:,1],-1)-points[:,1]*np.roll(points[:,0],-1)) for points in projected]
    outer=[i for i,area in enumerate(areas) if area>0];holes=[i for i,area in enumerate(areas) if area<0];owners={}
    for hole in holes:
        matches=[i for i in outer if rings[i].covers(rings[hole].representative_point())]
        if not matches:raise ValueError('Cut face has a hole without an outer boundary')
        owners[hole]=min(matches,key=lambda i:rings[i].area)
    faces=[]
    for index in outer:
        selected=[index]+[hole for hole in holes if owners[hole]==index]
        faces.append(replace(template,outer=loops[index],holes=tuple(loops[hole] for hole in selected[1:]),
            uvs=tuple(tuple(tuple(pair) for pair in uv[list(loops[number])]) for number in selected)))
    return tuple(faces)


def logical_faces(mesh,roots,keys,templates):
    groups={}
    for triangle,key in enumerate(keys):groups.setdefault(key,[]).append(triangle)
    faces=[];parts=np.full(len(keys),-1,dtype=np.int32);source=[]
    for key,rows in groups.items():
        template,source_index=templates[key];triangles=mesh['faces'][rows]
        recovered=polygon_faces(mesh['vertices'],mesh['uv'],triangles,roots,template)
        if len(recovered)==1:
            parts[rows]=len(faces);faces.append(recovered[0]);source.append(source_index);continue
        # Every triangle belongs to one recovered outer boundary, including disconnected fragments.
        for face in recovered:
            points=mesh['vertices'][list(face.outer)];shift=points-points[0]
            normal=np.cross(shift,np.roll(shift,-1,axis=0)).sum(axis=0);normal/=np.linalg.norm(normal)
            axis=np.eye(3)[np.argmin(abs(normal))];x=cross3(axis,normal);x/=np.linalg.norm(x);basis=np.array([x,cross3(normal,x)])
            polygon=shapely.Polygon((points-points[0])@basis.T,[(mesh['vertices'][list(ring)]-points[0])@basis.T for ring in face.holes])
            centres=mesh['vertices'][triangles].mean(axis=1);inside=shapely.covers(polygon,shapely.points((centres-points[0])@basis.T))
            selected=np.asarray(rows)[inside]
            if np.any(parts[selected]>=0):raise ValueError('Cut triangle belongs to multiple logical boundaries')
            parts[selected]=len(faces)
            faces.append(face);source.append(source_index)
    if np.any(parts<0):raise ValueError('Cut triangle is missing a logical boundary owner')
    mesh['parts']=parts
    return tuple(faces),tuple(source)
