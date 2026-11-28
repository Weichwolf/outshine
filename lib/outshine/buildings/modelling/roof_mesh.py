"""Assemble native roof triangles while retaining logical shared vertices and all corner UVs."""

from dataclasses import replace

import numpy as np

from .cut_topology import logical_faces,polygon_faces
from .faces import FACE_TAG_MASKS,Face,compile_faces
from .uv import name


class RoofMesh:
    def __init__(self,channels,projection_normal=None):
        self.channels=channels;self.properties=[];self.roots=[];self.keys=[];self.templates={};self.identifiers={}
        self.projection_normal=projection_normal

    def root(self,key):
        if key not in self.identifiers:self.identifiers[key]=len(self.identifiers)
        return self.identifiers[key]

    def cut_roots(self,slab):
        intersections={value:('ridge',*key) for key,value in slab.intersections.items()}
        return np.asarray([self.root(intersections.get(root,('source',root))) for root in slab.roots])

    def append(self,properties,roots,key,template):
        properties=np.asarray(properties);roots=np.asarray(roots)
        if len(properties)%3 or properties.shape!=(len(roots),3+2*len(self.channels)):
            raise ValueError('Roof triangle properties have inconsistent channel or corner counts')
        self.properties.append(properties);self.roots.append(roots);self.keys.extend([key]*(len(roots)//3))
        self.templates[key]=(replace(template,uv_sets=(),uvs=()),0)

    def mesh(self):
        data=np.concatenate(self.properties);mesh=dict(vertices=data[:,:3],faces=np.arange(len(data),dtype=np.int32).reshape(-1,3))
        mesh.update({name(channel):data[:,3+2*index:5+2*index] for index,channel in enumerate(self.channels)})
        return mesh,np.concatenate(self.roots)

    def bottom_faces(self):
        mesh,roots=self.mesh();selected=np.array([key=='bottom' for key in self.keys])
        return mesh,roots,polygon_faces(mesh['vertices'],mesh['uv'],mesh['faces'][selected],roots,self.templates['bottom'][0])

    def polygon(self,properties,roots,key,template):
        properties=np.asarray(properties);count=len(properties)
        if count<3:raise ValueError('Roof side has too few distinct corners')
        uv=tuple(tuple(pair) for pair in properties[:,3:5])
        extra=tuple((channel,(tuple(map(tuple,properties[:,3+2*index:5+2*index])),)) for index,channel in enumerate(self.channels) if channel)
        face=Face(tuple(range(count)),material=template.material,tags=template.tags,uvs=(uv,),group=template.group,uv_sets=extra)
        mesh,_=compile_faces(properties[:,:3],[face]);points={tuple(point):index for index,point in enumerate(properties[:,:3])}
        order=np.asarray([points[tuple(point)] for point in mesh['vertices'][mesh['faces']].reshape(-1,3)])
        self.append(properties[order],np.asarray(roots)[order],key,template)

    def finish(self):
        mesh,roots=self.mesh()
        projections=None if self.projection_normal is None else {key:(-self.projection_normal if key=='bottom' else self.projection_normal)
            for key in self.templates if key=='bottom' or isinstance(key,tuple) and key[0]=='top'}
        faces,_=logical_faces(mesh,roots,self.keys,self.templates,projections)
        faces=tuple(replace(face,uv_sets=tuple((channel,tuple(tuple(map(tuple,mesh[name(channel)][list(ring)]))
            for ring in (face.outer,*face.holes))) for channel in self.channels if channel)) for face in faces)
        unique,indices=np.unique(roots,return_index=True);prefix=mesh['vertices'][indices]
        mapping={root:number for number,root in enumerate(unique)};count=len(prefix)
        faces=tuple(replace(face,outer=tuple(mapping[roots[i]] for i in face.outer),
            holes=tuple(tuple(mapping[roots[i]] for i in ring) for ring in face.holes)) for face in faces)
        mesh['materials']=np.asarray([faces[i].material for i in mesh['parts']],dtype=np.uint16)
        mesh['groups']=np.asarray([faces[i].group for i in mesh['parts']],dtype=np.uint32)
        mesh['face_tag_mask']=np.asarray([sum(bit for tag,bit in FACE_TAG_MASKS.items() if tag in faces[i].tags) for i in mesh['parts']],dtype=np.uint8)
        mesh['vertices']=np.r_[prefix,mesh['vertices']];mesh['faces']+=count
        for channel in self.channels:mesh[name(channel)]=np.r_[np.zeros((count,2)),mesh[name(channel)]]
        return mesh,faces
