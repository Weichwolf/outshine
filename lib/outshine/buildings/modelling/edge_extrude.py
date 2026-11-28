"""Sweep native edges into side polygons, sharing only actual endpoint identities."""

from dataclasses import dataclass,replace

import numpy as np

from .extrude import aligned_scope,top_mapping
from .faces import EXTRUSION_TAGS,Face,face_shape
from .shape import Shape
from .metadata import inherit


@dataclass(frozen=True)
class EdgeSweepInfo:
    mode: str
    distance: float
    source_edges: int
    emitted_faces: int
    omitted_edges: tuple=()


def vertex_normals(vertices,edges,normals):
    summed=np.zeros_like(vertices)
    for edge,normal in zip(edges,normals,strict=True):summed[list(edge.vertices)]+=normal
    lengths=np.linalg.norm(summed,axis=1)
    used=sorted({vertex for edge in edges for vertex in edge.vertices})
    if np.any(lengths[used]<=1e-12):raise ValueError('Adjacent edge normals cancel at a vertex')
    return np.divide(summed,lengths[:,None],out=np.zeros_like(summed),where=lengths[:,None]!=0)


def targets(vertices,edges,normals,distance,mode):
    if mode=='vertex.normal':
        directions=vertex_normals(vertices,edges,normals)
        return vertices+directions*distance,directions[edges[0].vertices[0]]
    if mode=='world.up':return vertices+[0.,distance,0.],np.array([0.,1.,0.])
    if mode=='world.up.flatTop':
        span=max(abs(distance),float(np.ptp(vertices[:,1])))
        height=float(vertices[:,1].max()-span if distance<0 else vertices[:,1].min()+span)
        moved=vertices.copy();moved[:,1]=height
        return moved,np.array([0.,1.,0.])
    return None,normals[0]


def sweep_face(edge,mapping,points):
    a,b=edge.vertices;ring=(a,b,mapping[b],mapping[a])
    uv=tuple(edge.uvs[i] for i in (0,1,1,0)) if edge.uvs else ()
    # A flat-top endpoint may stay in place, leaving a valid triangle.
    kept=[i for i,index in enumerate(ring) if index!=ring[(i-1)%len(ring)]]
    ring=tuple(ring[i] for i in kept);uv=tuple(uv[i] for i in kept) if uv else ()
    if len(ring)<3:return None
    coordinates=np.asarray([points[i] for i in ring]);shifted=coordinates-coordinates[0]
    if np.linalg.norm(np.cross(shifted,np.roll(shifted,-1,axis=0)).sum(axis=0))<=1e-12:
        return None
    tags=(edge.tags-EXTRUSION_TAGS.keys())|{'extrude.side.outer'}
    return Face(ring,material=edge.material,tags=tags,uvs=(uv,) if uv else (),group=edge.group)


def extrude_edges(shape,distance,mode):
    mesh=shape.emit();edges=shape.geometry.edges;vertices=mesh['vertices']
    if shape.geometry.polygons or len(mesh['faces']) or shape.geometry.points:
        raise ValueError('Mixed face/edge/point extrusion needs an explicit assembly contract')
    if distance==0 and mode!='world.up.flatTop':return shape
    normals=mesh['edges_normals'];target,up=targets(vertices,edges,normals,distance,mode)
    if target is not None and np.array_equal(target,vertices):return shape
    points=vertices.tolist();known={};faces=[];omitted=[]
    for index,edge in enumerate(edges):
        selected=edge.vertices;normal=normals[index] if mode=='face.normal' else up
        top=vertices[list(selected)]+normal*distance if target is None else target[list(selected)]
        mapping={vertex:vertex for vertex,point in zip(selected,top,strict=True) if np.array_equal(point,vertices[vertex])}
        moved=[(vertex,point) for vertex,point in zip(selected,top,strict=True) if vertex not in mapping]
        if moved:mapping.update(top_mapping(points,[row[0] for row in moved],np.asarray([row[1] for row in moved]),normal,known))
        face=sweep_face(edge,mapping,points)
        if face is None:omitted.append((index,'zero-area sweep'))
        else:faces.append(face)
    points=np.asarray(points);scope=aligned_scope(points,up,shape.scope,distance<0)
    result=face_shape(points,faces,scope=scope,allow_warped=True) if faces else Shape.from_mesh(
        dict(vertices=points,faces=np.empty((0,3),dtype=np.int32),parts=np.empty(0,dtype=np.int32),materials=np.empty(0,dtype=np.uint16)),scope)
    return replace(inherit(result,shape),comp_info=shape.comp_info,sweep_info=EdgeSweepInfo(mode,distance,len(edges),len(faces),tuple(omitted)))
