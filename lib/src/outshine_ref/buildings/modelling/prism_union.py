"""Closed unions of metrical ENU prisms with stable construction/material origins."""

from dataclasses import dataclass
from numbers import Integral
from types import MappingProxyType

import numpy as np
import shapely

from .cut_topology import vertex_roots
from .mesh_fans import connected_fans
from .scope import scalar


@dataclass(frozen=True)
class PrismVolume:
    identifier: str
    polygon: object
    low_m: float
    high_m: float
    material: int=1

    def __post_init__(self):
        if not isinstance(self.identifier,str) or not self.identifier.strip():raise ValueError('Prism needs an explicit stable identifier')
        if not isinstance(self.polygon,shapely.Geometry) or self.polygon.geom_type not in ('Polygon','MultiPolygon') or not self.polygon.is_valid:
            raise ValueError('Prism needs a valid polygon or multipolygon')
        if shapely.get_coordinate_dimension(self.polygon)!=2:raise ValueError('Prism footprint must have two-dimensional coordinates')
        if not np.isfinite(shapely.get_coordinates(self.polygon)).all():raise ValueError('Prism coordinates must be finite')
        low,high=scalar(self.low_m),scalar(self.high_m)
        if high<=low:raise ValueError('Prism needs a positive height interval')
        if isinstance(self.material,bool) or not isinstance(self.material,Integral) or not 0<=self.material<=65535:
            raise ValueError('Prism material needs an unsigned 16-bit identifier')
        object.__setattr__(self,'low_m',low);object.__setattr__(self,'high_m',high)


@dataclass(frozen=True)
class PrismUnion:
    mesh: object
    vertex_roots: object
    source_ids: tuple
    source_indices: object
    source_triangles: object
    omissions: tuple
    volume_m3: float
    area_m2: float


def frozen(values):
    array=np.asarray(values)
    return np.frombuffer(array.tobytes(),dtype=array.dtype).reshape(array.shape)


def primitive_mesh(source):
    vertices=[];faces=[]
    for polygon in shapely.get_parts(shapely.orient_polygons(source.polygon)):
        for ring in (polygon.exterior,*polygon.interiors):
            for a,b in zip(list(ring.coords)[:-1],list(ring.coords)[1:],strict=True):
                first=len(vertices);vertices.extend([(*a,source.low_m),(*b,source.low_m),(*b,source.high_m),(*a,source.high_m)])
                faces.extend([(first,first+1,first+2),(first,first+2,first+3)])
        for triangle in shapely.get_parts(shapely.constrained_delaunay_triangles(polygon)):
            xy=np.asarray(triangle.exterior.coords)[:3]
            u,v=xy[1]-xy[0],xy[2]-xy[0]
            if u[0]*v[1]-u[1]*v[0]<0:xy=xy[::-1]
            for height,points in ((source.high_m,xy),(source.low_m,xy[::-1])):
                first=len(vertices);vertices.extend(np.c_[points,np.full(3,height)])
                faces.append((first,first+1,first+2))
    return np.asarray(vertices),np.asarray(faces,dtype=np.uint64)


def triangle_bound(source):
    return sum(4*sum(len(ring.coords)-1 for ring in (p.exterior,*p.interiors))+4*len(p.interiors)-4
               for p in shapely.get_parts(source.polygon) if not p.is_empty)


def union_prisms(sources,max_triangles=262144):
    """Emit a complete ENU boundary; procedural UVs and logical collision stay with the caller.

    Coordinates and heights are metres. Input order defines stable source table indices.
    This primitive constructor does not inherit arbitrary CGA Shape UVs/tags/material overrides.
    The triangle limits bound input/output cardinality, not wall time or all backend memory.
    """
    import manifold3d as manifold
    sources=tuple(sources)
    if type(max_triangles)is not int or max_triangles<1:raise ValueError('Prism triangle capacity must be a positive integer')
    if any(not isinstance(source,PrismVolume) for source in sources):raise ValueError('Union needs declared PrismVolume sources')
    if len({source.identifier for source in sources})!=len(sources):raise ValueError('Duplicate prism source identity')
    if sum(triangle_bound(source) for source in sources)>max_triangles:raise ValueError('Prism input triangle capacity exceeded')
    solids=[];identities={};omissions=[]
    for index,source in enumerate(sources):
        if source.polygon.is_empty:omissions.append((source.identifier,'empty_polygon'));continue
        vertices,faces=primitive_mesh(source)
        properties,triangles,duplicates,targets=connected_fans(dict(vertices=vertices,faces=faces,uv=np.zeros((len(vertices),2))),np.arange(len(faces)))
        identifier=manifold.Manifold.reserve_ids(1);identities[identifier]=index
        encoded=manifold.Mesh64(np.ascontiguousarray(properties[:,:3]),triangles,merge_from_vert=duplicates,merge_to_vert=targets,
            run_index=np.array([0,len(triangles)*3],dtype=np.uint64),run_original_id=np.array([identifier],dtype=np.uint32),
            face_id=np.arange(len(triangles),dtype=np.uint64))
        item=manifold.Manifold(encoded)
        if item.status()!=manifold.Error.NoError:raise ValueError('Prism source rejected: '+str(item.status()))
        solids.append(item)
    result=manifold.Manifold.batch_boolean(solids,manifold.OpType.Add)
    if result.status()!=manifold.Error.NoError:raise ValueError('Prism union rejected: '+str(result.status()))
    encoded=result.to_mesh64();faces=np.asarray(encoded.tri_verts,dtype=np.int32);properties=np.asarray(encoded.vert_properties)
    if len(faces)>max_triangles:raise ValueError('Prism output triangle capacity exceeded')
    roots=vertex_roots(encoded,len(properties));owners=np.full(len(faces),-1,dtype=np.int32)
    for first,last,identifier in zip(encoded.run_index[:-1],encoded.run_index[1:],encoded.run_original_id,strict=True):
        if first%3 or last%3:raise ValueError('Prism provenance run is not triangle-aligned')
        owners[int(first)//3:int(last)//3]=identities[int(identifier)]
    if np.any(owners<0):raise ValueError('Prism output has an undeclared source')
    materials=np.asarray([source.material for source in sources],dtype=np.uint16)[owners]
    mesh=MappingProxyType(dict(vertices=frozen(properties[:,:3]),faces=frozen(faces),materials=frozen(materials)))
    return PrismUnion(mesh,frozen(roots),tuple(source.identifier for source in sources),frozen(owners),
        frozen(np.asarray(encoded.face_id,dtype=np.int64)),tuple(omissions),result.volume(),result.surface_area())
