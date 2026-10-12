"""CGA face component selection, exclusion, connected groups and inherited shape metadata."""

from collections import deque
from dataclasses import dataclass
from types import MappingProxyType

import numpy as np

from modelling_component_geometry import component_shape,face_normals,topology
from modelling_selectors import select
from modelling_shape import CompInfo


@dataclass(frozen=True)
class Branch:
    selector: object='all'
    operator: str=':'

    def __post_init__(self):
        if self.operator not in (':','=','=:'):raise ValueError('A component operator must be :, = or =:')


@dataclass(frozen=True)
class FaceQuery:
    index: int
    face: object
    area: float
    normal: tuple
    scope_normal: tuple
    object_normal: tuple
    scope_size: tuple
    border: bool
    edge_tags: frozenset
    vertex_tags: frozenset


class Surface:
    def __init__(self,shape):
        self.source=shape;mesh=shape.emit()
        for array in mesh.values():array.setflags(write=False)
        self.mesh=MappingProxyType(mesh);self.faces,self.triangle_rows=topology(shape,mesh)
        self.normals=face_normals(mesh['vertices'],self.faces);self.normals.setflags(write=False)
        self.edge_owners,self.vertex_owners=self.owners()
        self.neighbours=tuple(frozenset(other for edge in self.face_edges(face) for other in self.edge_owners[edge] if other!=index)
                              for index,face in enumerate(self.faces))
        triangles=mesh['vertices'][mesh['faces']]
        area=np.linalg.norm(np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]),axis=1)/2
        self.areas=tuple(float(area[list(rows)].sum()) for rows in self.triangle_rows)

    @staticmethod
    def face_edges(face):
        return tuple(tuple(sorted((a,b))) for ring in (face.outer,*face.holes) for a,b in zip(ring,ring[1:]+ring[:1]))

    def owners(self):
        edges={};vertices={}
        for index,face in enumerate(self.faces):
            for edge in self.face_edges(face):edges.setdefault(edge,[]).append(index)
            for vertex in {i for ring in (face.outer,*face.holes) for i in ring}:vertices.setdefault(vertex,[]).append(index)
        return tuple(MappingProxyType({key:tuple(value) for key,value in rows.items()}) for rows in (edges,vertices))

    def query(self,index):
        face=self.faces[index];normal=self.normals[index];edges=self.face_edges(face)
        adjacent={other for edge in edges for other in self.edge_owners[edge]}
        touching={other for ring in (face.outer,*face.holes) for vertex in ring for other in self.vertex_owners[vertex]}
        edge_tags=frozenset(tag for other in adjacent for tag in self.faces[other].tags)
        vertex_tags=frozenset(tag for other in touching for tag in self.faces[other].tags)
        scope=self.source.scope
        return FaceQuery(index,face,self.areas[index],tuple(normal),tuple(normal@scope.axes),tuple(normal@scope.object_frame.axes),
                         scope.size,any(len(self.edge_owners[edge])==1 for edge in edges),edge_tags,vertex_tags)


def connected(surface,indices):
    remaining=set(indices);groups=[]
    for first in indices:
        if first not in remaining:continue
        remaining.remove(first);pending=deque([first]);found=[]
        while pending:
            current=pending.popleft();found.append(current)
            neighbours=sorted(surface.neighbours[current]&remaining)
            remaining.difference_update(neighbours);pending.extend(neighbours)
        groups.append(tuple(sorted(found)))
    return tuple(groups)


def split_components(shape,component,branches,alignment):
    if component!='f':raise NotImplementedError('Only face components are implemented; edge/vertex/group/material/hole components remain pending')
    if alignment not in ('zUp','noAlign'):raise ValueError('Unknown component scope alignment')
    branches=branches or (Branch(),)
    if any(not isinstance(branch,Branch) for branch in branches):raise ValueError('Component rules require Branch objects')
    surface=Surface(shape);queries=tuple(surface.query(i) for i in range(len(surface.faces)));remaining=set(range(len(queries)));products=[]
    for branch in branches:
        selector=select(branch.selector)
        indices=tuple(i for i in range(len(queries)) if i in remaining and selector(queries[i]));remaining.difference_update(indices)
        if not indices:continue
        groups=tuple((i,) for i in indices) if branch.operator==':' else (indices,) if branch.operator=='=' else connected(surface,indices)
        products.extend(component_shape(surface,group,alignment,CompInfo(selector.name,index,len(groups),group)) for index,group in enumerate(groups))
    return tuple(products)
