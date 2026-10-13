"""Extract unique and face-relative lines/points without confusing their normals or topology."""

from dataclasses import dataclass,replace

import numpy as np

from modelling_component_geometry import fit_scope
from modelling_extrude import euler
from outshine_ref.buildings.modelling.scope import Frame
from modelling_wire import Edge,Point,wire_shape
from modelling_metadata import inherit


MINIMUM_UNIQUE_EDGE_M=.0008


def unit(vector):
    length=np.linalg.norm(vector)
    if length<=1e-12:raise ValueError('Component direction has no reliable normal or tangent')
    return vector/length


def vertex_tangent(normal):
    reference=np.array([1.,0.,0.] if abs(normal[0])<=.999999 else [0.,1.,0.])
    return unit(np.cross(normal,reference))


@dataclass(frozen=True)
class WireRecord:
    element: object
    normal: tuple
    tangent: tuple
    border: bool
    vertex_tags: frozenset


def edge_records(surface,kind):
    vertices=surface.mesh['vertices'];records=[];seen=set()
    vertex_tags={v:frozenset(tag for owner in owners for tag in surface.faces[owner].tags) for v,owners in surface.vertex_owners.items()}
    for index,face in enumerate(surface.faces):
        for ring_index,ring in enumerate((face.outer,*face.holes)):
            for corner,(a,b) in enumerate(zip(ring,ring[1:]+ring[:1])):
                key=tuple(sorted((a,b)))
                if kind=='e' and key in seen:continue
                seen.add(key);vector=vertices[b]-vertices[a];length=np.linalg.norm(vector)
                if length<MINIMUM_UNIQUE_EDGE_M and kind=='e':continue
                tangent=unit(vector);owners=surface.edge_owners[key]
                normal=unit(sum((surface.normals[owner] for owner in owners),np.zeros(3))) if kind=='e' else unit(np.cross(tangent,surface.normals[index]))
                if kind=='e':normal=unit(normal-tangent*np.dot(normal,tangent))
                tags=frozenset(tag for owner in owners for tag in surface.faces[owner].tags)
                uv=face.uvs[ring_index] if face.uvs else ()
                element=Edge((a,b),face.material,tags,(uv[corner],uv[(corner+1)%len(ring)]) if uv else (),face.group)
                records.append(WireRecord(element,tuple(normal),tuple(tangent),len(owners)==1,vertex_tags[a]|vertex_tags[b]))
    return tuple(records)


def point_records(surface,kind):
    vertices=surface.mesh['vertices'];records=[]
    border_vertices={v for edge,owners in surface.edge_owners.items() if len(owners)==1 for v in edge}
    tags={v:frozenset(tag for owner in owners for tag in surface.faces[owner].tags) for v,owners in surface.vertex_owners.items()}
    if kind=='v':
        for vertex,owners in sorted(surface.vertex_owners.items()):
            normal=unit(sum((surface.normals[owner] for owner in owners),np.zeros(3)));tangent=vertex_tangent(normal)
            face=surface.faces[owners[0]];element=Point((vertex,),face.material,tags[vertex],group=face.group)
            records.append(WireRecord(element,tuple(normal),tuple(tangent),vertex in border_vertices,tags[vertex]))
        return tuple(records)
    for index,face in enumerate(surface.faces):
        for ring_index,ring in enumerate((face.outer,*face.holes)):
            for corner,current in enumerate(ring):
                before,after=ring[(corner-1)%len(ring)],ring[(corner+1)%len(ring)]
                tangent=unit(unit(vertices[current]-vertices[before])+unit(vertices[after]-vertices[current]))
                normal=unit(np.cross(tangent,surface.normals[index]));uv=face.uvs[ring_index] if face.uvs else ()
                element=Point((current,),face.material,tags[current],(uv[corner],) if uv else (),face.group)
                records.append(WireRecord(element,tuple(normal),tuple(tangent),current in border_vertices,tags[current]))
    return tuple(records)


def existing_records(shape,mesh,kind):
    edges=shape.geometry.edges;points=shape.geometry.points;positions=mesh['vertices'];records=[]
    if edges:
        owners={}
        for index,edge in enumerate(edges):
            for vertex in edge.vertices:owners.setdefault(vertex,[]).append(index)
        tags={v:frozenset(tag for owner in row for tag in edges[owner].tags) for v,row in owners.items()}
        if kind=='v':
            for vertex,row in sorted(owners.items()):
                normal=unit(mesh['edges_normals'][row].sum(axis=0));tangent=vertex_tangent(normal)
                records.append(WireRecord(Point((vertex,),edges[row[0]].material,tags[vertex],group=edges[row[0]].group),tuple(normal),tuple(tangent),False,tags[vertex]))
        else:
            seen=set()
            for index,edge in enumerate(edges):
                a,b=edge.vertices;tangent=unit(positions[b]-positions[a]);normal=mesh['edges_normals'][index]
                key=tuple(sorted((a,b)))
                if kind=='native':records.append(WireRecord(edge,tuple(normal),tuple(tangent),False,tags[a]|tags[b]))
                elif kind=='e':
                    if key in seen:continue
                    seen.add(key);records.append(WireRecord(edge,tuple(normal),tuple(tangent),False,tags[a]|tags[b]))
                elif kind=='fe':
                    for first,last,sign in ((a,b,1.),(b,a,-1.)):
                        x=tangent*sign;z=unit(np.cross(x,normal));uv=edge.uvs if sign>0 else edge.uvs[::-1]
                        records.append(WireRecord(replace(edge,vertices=(first,last),uvs=uv),tuple(z),tuple(x),False,tags[a]|tags[b]))
                elif kind=='fv':
                    z=unit(np.cross(tangent,normal))
                    for corner,vertex in enumerate(edge.vertices):
                        element=Point((vertex,),edge.material,tags[vertex],(edge.uvs[corner],) if edge.uvs else (),edge.group)
                        records.append(WireRecord(element,tuple(z),tuple(tangent),False,tags[vertex]))
    elif points and kind in ('v','native'):
        for index,point in enumerate(points):
            normal=mesh['points_normals'][index];records.append(WireRecord(point,tuple(normal),tuple(vertex_tangent(normal)),False,point.tags))
    return tuple(records)


class WireSet:
    def __init__(self,shape,kind):
        from modelling_components import Surface
        self.source=shape;self.kind=kind
        if shape.geometry.polygons or len(shape.geometry.fields['faces']):
            surface=Surface(shape);self.mesh=surface.mesh
            self.records=edge_records(surface,kind) if kind in ('e','fe') else point_records(surface,kind)
        else:
            self.mesh=shape.emit();self.records=existing_records(shape,self.mesh,kind)
        self.vertices=self.mesh['vertices'];owners={}
        for index,record in enumerate(self.records):
            for vertex in record.element.vertices:owners.setdefault(vertex,set()).add(index)
        self.neighbours=tuple(frozenset(other for vertex in record.element.vertices for other in owners[vertex] if other!=index) for index,record in enumerate(self.records))

    def query(self,index):
        from modelling_components import ComponentQuery
        record=self.records[index];normal=np.asarray(record.normal);scope=self.source.scope
        length=float(np.linalg.norm(np.diff(self.vertices[list(record.element.vertices)],axis=0))) if isinstance(record.element,Edge) else 0.
        return ComponentQuery(index,record.element,0.,tuple(normal),tuple(normal@scope.axes),tuple(normal@scope.object_frame.axes),
                              scope.size,record.border,record.element.tags,record.vertex_tags,length)


def wire_component_shape(surface,indices,alignment,info):
    records=[surface.records[index] for index in indices];elements=tuple(record.element for record in records)
    used=sorted({v for element in elements for v in element.vertices});mapping={old:new for new,old in enumerate(used)}
    points=surface.vertices[used];elements=tuple(replace(element,vertices=tuple(mapping[i] for i in element.vertices)) for element in elements)
    old=surface.source.scope;origin=points[elements[0].vertices[0]]
    if alignment=='noAlign':scope=fit_scope(points,old.axes,old.origin,old,signed=True)
    else:
        x=unit(np.asarray(records[0].tangent));normal=np.asarray(records[0].normal);z=unit(normal-x*np.dot(normal,x))
        axes=np.column_stack((x,np.cross(z,x),z));pivot=Frame(tuple(old.object_frame.axes.T@(origin-old.object_frame.position)),euler(old.object_frame.axes.T@axes))
        scope=fit_scope(points,axes,origin,old,pivot)
    result=wire_shape(points,elements,[record.normal for record in records],scope)
    return replace(inherit(result,surface.source),comp_info=info)
