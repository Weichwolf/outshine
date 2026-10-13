"""Immutable polygon boundaries with holes, per-face UVs and GEOS native triangulation."""

from dataclasses import dataclass,replace
from numbers import Integral

import numpy as np
import shapely

from outshine_ref.buildings.modelling.scope import scalar
from modelling_metadata import group_id
from modelling_uv_corners import rings_uv,channels_uv


EXTRUSION_TAGS={'extrude.bottom':1,'extrude.side.outer':2,'extrude.side.inner':4,'extrude.top':8}
ROOF_TAGS={'roof.bottom':16,'roof.side.outer':32,'roof.side.inner':64,'roof.top':128}
FACE_TAG_MASKS=EXTRUSION_TAGS|ROOF_TAGS


def ring_indices(values):
    values=tuple(values)
    if any(isinstance(value,bool) or not isinstance(value,Integral) for value in values):
        raise ValueError('Polygon boundaries require integer vertex indices')
    values=tuple(int(value) for value in values)
    if len(values)>1 and values[0]==values[-1]:values=values[:-1]
    if len(values)<3 or len(set(values))!=len(values):raise ValueError('Polygon ring has repeated or too few vertices')
    return values


@dataclass(frozen=True)
class Face:
    outer: tuple
    holes: tuple=()
    material: int=1
    tags: frozenset=frozenset()
    uvs: tuple=()
    group: int=0
    uv_sets: tuple=()

    def __post_init__(self):
        object.__setattr__(self,'outer',ring_indices(self.outer))
        object.__setattr__(self,'group',group_id(self.group))
        object.__setattr__(self,'holes',tuple(ring_indices(ring) for ring in self.holes))
        if isinstance(self.material,bool) or not isinstance(self.material,Integral) or not 0<=self.material<=65535:
            raise ValueError('Polygon material requires an unsigned 16-bit identifier')
        object.__setattr__(self,'tags',frozenset(self.tags))
        if any(not isinstance(tag,str) for tag in self.tags):raise ValueError('Face tags must be strings')
        if self.uvs:
            object.__setattr__(self,'uvs',rings_uv(self.uvs,(self.outer,*self.holes)))
        object.__setattr__(self,'uv_sets',channels_uv(self.uv_sets,(self.outer,*self.holes)))


def plane(vertices,face,allow_warped=False):
    if any(index<0 or index>=len(vertices) for ring in (face.outer,*face.holes) for index in ring):
        raise ValueError('Polygon boundary references a missing vertex')
    points=vertices[list(face.outer)];origin=points[0];shifted=points-origin
    vector=np.cross(shifted,np.roll(shifted,-1,axis=0)).sum(axis=0)
    length=np.linalg.norm(vector)
    if length<=1e-12:raise ValueError('Polygon face has no reliable area normal')
    normal=vector/length
    edges=np.roll(points,-1,axis=0)-points
    tangent=edges[np.argmax(np.linalg.norm(edges,axis=1))]
    tangent=tangent-normal*np.dot(tangent,normal);tangent/=np.linalg.norm(tangent)
    across=np.cross(normal,tangent);basis=np.array([tangent,across])
    rings=[np.array(ring,dtype=np.int32) for ring in (face.outer,*face.holes)]
    tolerance=max(1e-9,np.linalg.norm(points-origin,axis=1).max()*1e-10)
    if not allow_warped and any(np.max(abs((vertices[ring]-origin)@normal))>tolerance for ring in rings):
        raise ValueError('Polygon constructor requires planar faces')
    coordinates=[(vertices[ring]-origin)@basis.T for ring in rings]
    polygon=shapely.Polygon(coordinates[0],coordinates[1:])
    if not polygon.is_valid or polygon.area<=1e-12:raise ValueError('Invalid polygon face: '+shapely.is_valid_reason(polygon))
    uv=face.uvs or tuple(tuple(tuple(pair) for pair in xy) for xy in coordinates)
    holes=list(face.holes);uv=list(uv);extra={channel:list(rows) for channel,rows in face.uv_sets}
    for index,xy in enumerate(coordinates[1:],1):
        next_xy=np.roll(xy,-1,axis=0)
        area=np.sum(xy[:,0]*next_xy[:,1]-xy[:,1]*next_xy[:,0])
        if area>0:
            holes[index-1]=holes[index-1][::-1];coordinates[index]=xy[::-1];uv[index]=uv[index][::-1]
            for rows in extra.values():rows[index]=rows[index][::-1]
    return normal,polygon,coordinates,replace(face,holes=tuple(holes),uvs=tuple(uv),uv_sets=tuple((channel,tuple(rows)) for channel,rows in extra.items()))


def triangulations(polygons):
    return shapely.constrained_delaunay_triangles(np.asarray(polygons,dtype=object))


def compile_faces(vertices,faces,vertex_normals=None,allow_warped=False):
    points=np.asarray(vertices,dtype=np.float64)
    if points.ndim!=2 or points.shape[1]!=3 or not np.isfinite(points).all():raise ValueError('Polygon geometry requires finite XYZ vertices')
    if vertex_normals is not None:
        vertex_normals=np.asarray(vertex_normals,dtype=np.float64)
        if vertex_normals.shape!=points.shape or not np.isfinite(vertex_normals).all():raise ValueError('Invalid authored polygon vertex normals')
    output=points.tolist();uvs=[[0.,0.]]*len(points);triangles=[];materials=[];parts=[];masks=[];groups=[]
    normals=vertex_normals.tolist() if vertex_normals is not None else None
    if any(not isinstance(face,Face) for face in faces):raise ValueError('Logical polygon boundaries require Face objects')
    prepared=[plane(points,face,allow_warped) for face in faces]
    extra={channel:[[0.,0.]]*len(points) for _,_,_,face in prepared for channel,_ in face.uv_sets}
    triangulated=triangulations([item[1] for item in prepared]);topology=[]
    for number,(_,polygon,coordinates,face) in enumerate(prepared):
        topology.append(face)
        corners={tuple(xy):(index,face.uvs[r][i]) for r,ring in enumerate((face.outer,*face.holes))
                 for i,(index,xy) in enumerate(zip(ring,coordinates[r]))}
        extra_corners={channel:{tuple(xy):values[r][i] for r,xy_ring in enumerate(coordinates) for i,xy in enumerate(xy_ring)}
                       for channel,values in face.uv_sets}
        tag_mask=sum(bit for name,bit in FACE_TAG_MASKS.items() if name in face.tags)
        for triangle in shapely.get_parts(triangulated[number]):
            xy=np.array(triangle.exterior.coords)[:3]
            a,b=xy[1]-xy[0],xy[2]-xy[0]
            if a[0]*b[1]-a[1]*b[0]<0:xy=xy[::-1]
            first=len(output)
            for pair in xy:
                index,uv=corners[tuple(pair)];output.append(points[index].tolist());uvs.append(uv)
                for channel,values in extra.items():values.append(extra_corners.get(channel,{}).get(tuple(pair),(0.,0.)))
                if normals is not None:normals.append(vertex_normals[index].tolist())
            triangles.append([first,first+1,first+2]);materials.append(face.material);parts.append(number);masks.append(tag_mask);groups.append(face.group)
    mesh=dict(vertices=np.asarray(output,dtype=np.float64).reshape(-1,3),faces=np.asarray(triangles,dtype=np.int32).reshape(-1,3),
              uv=np.asarray(uvs,dtype=np.float64).reshape(-1,2),materials=np.asarray(materials,dtype=np.uint16),
              parts=np.asarray(parts,dtype=np.int32),face_tag_mask=np.asarray(masks,dtype=np.uint8),groups=np.asarray(groups,dtype=np.uint32))
    if normals is not None:mesh['normals']=np.asarray(normals)
    for channel,values in extra.items():mesh['uv'+str(channel)]=np.asarray(values,dtype=np.float64).reshape(-1,2)
    return mesh,tuple(topology)


def face_shape(vertices,faces,scope=None,vertex_normals=None,allow_warped=False):
    from modelling_shape import Shape
    mesh,topology=compile_faces(vertices,faces,vertex_normals,allow_warped)
    return Shape.from_mesh(mesh,scope,topology)


def polygon_shape(polygon,elevation=0.,material=1,tags=()):
    if not isinstance(polygon,shapely.Polygon) or not polygon.is_valid or polygon.is_empty:
        raise ValueError('Footprint constructor requires a valid nonempty Polygon')
    polygon=shapely.orient_polygons(polygon);vertices=[];rings=[];elevation=scalar(elevation)
    for ring in (polygon.exterior,*polygon.interiors):
        indices=[]
        for x,north in list(ring.coords)[:-1]:
            indices.append(len(vertices));vertices.append([x,elevation,-north])
        rings.append(tuple(indices))
    return face_shape(vertices,[Face(rings[0],tuple(rings[1:]),material,frozenset(tags))])
