"""Recover source-aware logical regions without changing native interpolation domains."""

from dataclasses import dataclass,replace

import numpy as np

from outshine_ref.buildings.modelling.cut_topology import polygon_faces
from outshine_ref.buildings.modelling.faces import Face,FACE_TAG_MASKS


@dataclass(frozen=True)
class LogicalRegions:
    faces: tuple
    parts: object
    supports: tuple
    rows: tuple


def source_faces(result):
    product=result.product;keys=[]
    for supports in result.supports:
        keys.append(tuple((owner,int(product.sources[owner].mesh['parts'][number])) for owner,number in supports))
    return tuple(keys)


def connected_rows(product,keys):
    parent=np.arange(len(keys));edges={};completed=set()

    def root(index):
        while parent[index]!=index:
            parent[index]=parent[parent[index]];index=int(parent[index])
        return index

    for number,triangle in enumerate(product.roots[product.mesh['faces']]):
        for a,b in zip(triangle,np.roll(triangle,-1),strict=True):
            edge=(int(a),int(b));opposite=edge[::-1]
            undirected=tuple(sorted(edge))
            if undirected in completed:raise ValueError('Logical assembly found a nonmanifold edge')
            if opposite in edges:
                completed.add(undirected)
                other=edges.pop(opposite)
                if keys[number]==keys[other]:
                    first,last=sorted((root(number),root(other)));parent[last]=first
            else:
                if edge in edges:raise ValueError('Logical assembly found a duplicate directed edge')
                edges[edge]=number
    if edges:raise ValueError('Logical assembly requires closed directed native surfaces')
    groups={}
    for index in range(len(keys)):groups.setdefault(root(index),[]).append(index)
    return tuple(tuple(rows) for rows in groups.values())


def recover(result,bound):
    product=result.product;mesh=product.mesh;supports=source_faces(result)
    keys=tuple((support,int(bound.materials[i]),int(bound.groups[i]),bound.tags[i]) for i,support in enumerate(supports))
    components=connected_rows(product,keys);faces=[];parts=np.full(len(keys),-1,dtype=np.int32)
    roots={}
    for index,root in enumerate(product.roots):roots.setdefault(int(root),index)
    ancestry=[];owners=[]
    for rows in components:
        first=rows[0];template=Face((0,1,2),material=int(bound.materials[first]),group=int(bound.groups[first]),tags=bound.tags[first])
        recovered=polygon_faces(mesh['vertices'],mesh['uv'],mesh['faces'][list(rows)],product.roots,template)
        if len(recovered)!=1:raise ValueError('One connected semantic patch needs exactly one outer logical region')
        face=recovered[0];rings=(face.outer,*face.holes)
        channels=tuple((c,tuple(tuple(tuple(pair) for pair in mesh['uv'+str(c)][list(ring)]) for ring in rings)) for c in range(1,10))
        remapped=tuple(tuple(roots[int(product.roots[i])] for i in ring) for ring in rings)
        faces.append(replace(face,outer=remapped[0],holes=remapped[1:],uv_sets=channels))
        parts[list(rows)]=len(faces)-1;ancestry.append(supports[first]);owners.append(rows)
    if np.any(parts<0):raise ValueError('Native triangle lacks a logical semantic region')
    parts=np.frombuffer(parts.tobytes(),dtype=parts.dtype)
    return LogicalRegions(tuple(faces),parts,tuple(ancestry),tuple(owners))


def mesh_fields(product,bound,regions):
    mesh=dict(product.mesh,parts=regions.parts,materials=bound.materials,groups=bound.groups)
    mesh['face_tag_mask']=np.asarray([sum(value for tag,value in FACE_TAG_MASKS.items() if tag in tags) for tags in bound.tags],dtype=np.uint8)
    return mesh
