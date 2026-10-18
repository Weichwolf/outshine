"""Prism openings in one closed ENU surface with explicit triangle/material provenance."""

from dataclasses import dataclass,replace
from types import MappingProxyType

import numpy as np
from shapely.affinity import translate

from .cut_topology import vertex_roots
from .mesh_fans import connected_fans
from .prism_union import PrismVolume,frozen,primitive_mesh,triangle_bound
from .cut_normals import triangle_normals


@dataclass(frozen=True)
class PrismCut:
    mesh: object
    vertex_roots: object
    source_ids: tuple
    source_indices: object
    source_triangles: object
    omissions: tuple
    volume_m3: float
    area_m2: float


def encoded(mesh,roots,origin,identifier,channels):
    import manifold3d as manifold
    vertices=np.asarray(mesh['vertices'],dtype=np.float64);faces=np.asarray(mesh['faces'])
    if vertices.ndim!=2 or vertices.shape[1]!=3 or not np.isfinite(vertices).all():raise ValueError('Cut surface requires finite ENU vertices')
    if faces.ndim!=2 or faces.shape[1]!=3 or faces.dtype.kind not in 'iu' or not len(faces):raise ValueError('Cut surface requires nonempty integer triangles')
    if faces.min()<0 or faces.max()>=len(vertices):raise ValueError('Cut triangle references a missing vertex')
    roots=np.asarray(roots)
    if roots.shape!=(len(vertices),) or roots.dtype.kind not in 'iu' or np.any(roots<0):raise ValueError('Cut surface requires explicit integer topology roots')
    _,first,inverse=np.unique(roots,return_index=True,return_inverse=True);targets=first[inverse]
    if not np.array_equal(vertices,vertices[targets]):raise ValueError('Cut topology root joins different positions')
    indices=np.arange(len(vertices));merged=indices!=targets;values=[vertices-origin]
    for channel in channels:
        value=np.asarray(mesh.get(channel,np.zeros((len(vertices),2))),dtype=np.float64)
        if value.shape!=(len(vertices),2) or not np.isfinite(value).all():raise ValueError('Cut UV channel requires finite vertex pairs')
        values.append(value)
    properties=np.ascontiguousarray(np.c_[tuple(values)])
    item=manifold.Mesh64(properties,np.asarray(faces,dtype=np.uint64,order='C'),
        merge_from_vert=indices[merged].astype(np.uint64),merge_to_vert=targets[merged].astype(np.uint64),
        run_index=np.array([0,len(faces)*3],dtype=np.uint64),run_original_id=np.array([identifier],dtype=np.uint32),
        face_id=np.arange(len(faces),dtype=np.uint64))
    solid=manifold.Manifold(item)
    if solid.status()!=manifold.Error.NoError:raise ValueError('Cut source rejected: '+str(solid.status()))
    return solid


def prism_source(source,origin,identifier,channels):
    local=replace(source,polygon=translate(source.polygon,xoff=-origin[0],yoff=-origin[1]))
    vertices,faces=primitive_mesh(local)
    points,triangles,duplicates,targets=connected_fans(dict(vertices=vertices,faces=faces,uv=np.zeros((len(vertices),2))),np.arange(len(faces)))
    roots=np.arange(len(points),dtype=np.uint64);roots[duplicates]=targets
    mesh=dict(vertices=points[:,:3],faces=triangles)
    return encoded(mesh,roots,np.zeros(3),identifier,channels),len(triangles)


def decoded(solid,mesh,sources,names,ids,counts,origin,channels,max_triangles,omissions):
    data=solid.to_mesh64();properties=np.asarray(data.vert_properties);faces=np.asarray(data.tri_verts,dtype=np.int32)
    if not len(faces):properties=np.empty((0,3+2*len(channels)),dtype=np.float64)
    if len(faces)>max_triangles:raise ValueError('Cut output triangle capacity exceeded')
    owners=np.full(len(faces),-1,dtype=np.int32);references=np.asarray(data.face_id,dtype=np.int64)
    for first,last,identifier in zip(data.run_index[:-1],data.run_index[1:],data.run_original_id,strict=True):
        if first%3 or last%3 or int(identifier) not in ids:raise ValueError('Cut output has undeclared triangle provenance')
        owners[int(first)//3:int(last)//3]=ids[int(identifier)]
    if len(references)!=len(faces) or np.any(owners<0):raise ValueError('Cut source relation is incomplete')
    if np.any(references<0) or any(np.any(references[owners==i]>=counts[i]) for i in range(len(counts))):
        raise ValueError('Cut output refers beyond its original source triangulation')
    materials=np.zeros(len(faces),dtype=np.uint16)
    selected=owners==0;materials[selected]=np.asarray(mesh.get('materials',np.ones(len(mesh['faces']),dtype=np.uint16)))[references[selected]]
    for index,source in enumerate(sources,1):materials[owners==index]=source.material
    vertices=properties[:,:3]+origin
    output=dict(vertices=vertices,faces=faces,materials=materials)
    for number,channel in enumerate(channels):output[channel]=properties[:,3+2*number:5+2*number]
    normals=triangle_normals(vertices,faces,mesh,sources,owners,references)
    corner_faces=np.arange(faces.size,dtype=np.int32).reshape(-1,3)
    old_roots=vertex_roots(data,len(vertices));roots=old_roots[faces].reshape(-1)
    output={key:value[faces].reshape(-1,value.shape[-1]) for key,value in output.items() if key not in ('faces','materials')}
    output.update(faces=corner_faces,materials=materials,normals=np.repeat(normals,3,axis=0))
    return PrismCut(MappingProxyType({key:frozen(value) for key,value in output.items()}),frozen(roots),
        names,frozen(owners),frozen(references),tuple(omissions),solid.volume(),solid.surface_area())


def subtract_prisms(surface_id,surface,sources,max_triangles=262144):
    """Cut declared vertical prisms from a closed ENU mesh with explicit topology roots.

    Source index zero names the original surface; later indices preserve cutter order.
    Surface UV0–9 interpolate from original triangles, cut UVs are explicit zeros and
    cut materials follow their PrismVolume. Geometric normals are rebuilt; exact
    null topology facets retain their oriented parent normal without moving vertices.
    Render surfaces may omit those area-free facets; closed topology retains them. Arbitrary
    Shape resources/groups/tags/authored normals and recursive Boolean semantics remain
    outside this primitive. Cardinality limits do not prove a backend time/memory bound.
    """
    import manifold3d as manifold
    if not isinstance(surface_id,str) or not surface_id.strip():raise ValueError('Cut surface needs an explicit stable identity')
    sources=tuple(sources)
    if any(not isinstance(source,PrismVolume) for source in sources):raise ValueError('Cuts require PrismVolume sources')
    names=(surface_id,*(source.identifier for source in sources))
    if len(set(names))!=len(names):raise ValueError('Duplicate cut source identity')
    if type(max_triangles)is not int or max_triangles<1:raise ValueError('Cut triangle capacity must be a positive integer')
    if not hasattr(surface,'mesh') or not hasattr(surface,'vertex_roots'):raise ValueError('Cut surface requires mesh and topology roots')
    mesh=surface.mesh;roots=surface.vertex_roots
    allowed={'vertices','faces','materials','parts','groups','face_tag_mask','normals','uv'}|{'uv'+str(i) for i in range(1,10)}
    if set(mesh)-allowed:raise NotImplementedError('Uncontracted cut surface property channel')
    if len(mesh['faces'])+sum(triangle_bound(source) for source in sources)>max_triangles:
        raise ValueError('Cut input triangle capacity exceeded')
    materials=np.asarray(mesh.get('materials',np.ones(len(mesh['faces']),dtype=np.uint16)))
    if materials.shape!=(len(mesh['faces']),) or materials.dtype.kind not in 'iu' or np.any(materials<0) or np.any(materials>65535):
        raise ValueError('Invalid cut surface material binding')
    channels=tuple(['uv']+[name for name in ('uv'+str(i) for i in range(1,10)) if name in mesh])
    points=np.asarray(mesh['vertices'],dtype=np.float64)
    if points.ndim!=2 or points.shape[1]!=3 or not len(points) or not np.isfinite(points).all():
        raise ValueError('Cut surface requires nonempty finite ENU vertices')
    origin=np.r_[points[:,:2].min(axis=0)+(points[:,:2].max(axis=0)-points[:,:2].min(axis=0))/2,0.]
    identifiers=manifold.Manifold.reserve_ids(len(sources)+1);ids={identifiers+i:i for i in range(len(sources)+1)}
    counts=[len(mesh['faces'])];outer=encoded(mesh,roots,origin,identifiers,channels);cutters=[];omissions=[]
    for index,source in enumerate(sources,1):
        if source.polygon.is_empty:omissions.append((source.identifier,'empty_polygon'));counts.append(0);continue
        item,count=prism_source(source,origin,identifiers+index,channels);cutters.append(item);counts.append(count)
    cutter=manifold.Manifold.batch_boolean(cutters,manifold.OpType.Add)
    result=outer-cutter
    if result.status()!=manifold.Error.NoError:raise ValueError('Prism subtraction rejected: '+str(result.status()))
    return decoded(result,mesh,sources,names,ids,counts,origin,channels,max_triangles,omissions)
