"""Closed-solid Boolean experiment with all UV channels and native triangle provenance."""

from dataclasses import dataclass,replace

import manifold3d as manifold
import numpy as np

from outshine_ref.buildings.modelling.cut_source import closed_components
from outshine_ref.buildings.modelling.cut_topology import vertex_roots
from outshine_ref.buildings.modelling.mesh_fans import connected_fans
from outshine_ref.buildings.modelling import Frame


@dataclass(frozen=True)
class BooleanSource:
    shape: object
    mesh: dict
    solid: object
    original_id: int


@dataclass(frozen=True)
class BooleanProduct:
    mesh: dict
    operands: object
    source_triangles: object
    sources: tuple
    roots: object
    zero_volume_contacts: tuple
    object_frame: object
    backend_volume_m3: float
    backend_area_m2: float


def local_mesh(shape,common):
    working=replace(shape,scope=replace(shape.scope,object_frame=Frame()))
    mesh=working.emit();frame=shape.scope.object_frame
    if frame!=common:
        mesh['vertices']=mesh['vertices']@(frame.axes.T@common.axes)+(np.asarray(frame.position)-common.position)@common.axes
    return mesh


def encoded_source(shape,identifier,anchor,max_triangles,common):
    mesh=local_mesh(shape,common)
    if shape.geometry.edges or shape.geometry.points or not shape.geometry.polygons:
        raise ValueError('Boolean experiment requires logical closed surfaces')
    if not 0<len(mesh['faces'])<=max_triangles:raise ValueError('Boolean input triangle capacity exceeded')
    if not closed_components(mesh):raise ValueError('Boolean experiment requires closed operands')
    allowed={'vertices','faces','parts','materials','groups','face_tag_mask','normals','uv'}|{'uv'+str(i) for i in range(1,10)}
    if set(mesh)-allowed:raise ValueError('Uncontracted Boolean property stream')
    native=dict(mesh);native['vertices']=mesh['vertices']-anchor
    native.setdefault('uv',np.zeros((len(mesh['vertices']),2)))
    properties,triangles,duplicates,targets=connected_fans(native,np.arange(len(mesh['faces'])))
    streams=[properties]
    for channel in range(1,10):
        values=mesh.get('uv'+str(channel),np.zeros((len(mesh['vertices']),2)))
        streams.append(values[mesh['faces']].reshape(-1,2))
    properties=np.ascontiguousarray(np.c_[tuple(streams)],dtype=np.float64)
    encoded=manifold.Mesh64(properties,triangles,merge_from_vert=duplicates,merge_to_vert=targets,
        run_index=np.array([0,len(triangles)*3],dtype=np.uint64),run_original_id=np.array([identifier],dtype=np.uint32),
        face_id=np.arange(len(triangles),dtype=np.uint64))
    solid=manifold.Manifold(encoded)
    if solid.status()!=manifold.Error.NoError or solid.is_empty():raise ValueError('Boolean source rejected: '+str(solid.status()))
    return BooleanSource(shape,mesh,solid,identifier)


def decode(solid,sources,identifier,anchor,max_triangles):
    encoded=solid.to_mesh64();properties=np.asarray(encoded.vert_properties);triangles=np.asarray(encoded.tri_verts,dtype=np.int32)
    if len(triangles)>max_triangles:raise ValueError('Boolean output triangle capacity exceeded')
    if not len(triangles):
        mesh=dict(vertices=np.empty((0,3)),faces=np.empty((0,3),dtype=np.int32))
        return mesh,np.empty(0,dtype=np.int32),np.empty(0,dtype=np.int32),np.empty(0,dtype=np.int32)
    if properties.shape[1]!=23 or not np.isfinite(properties).all():raise ValueError('Boolean property layout changed')
    owners=np.full(len(triangles),-1,dtype=np.int32);references=np.asarray(encoded.face_id,dtype=np.int64)
    for start,stop,original in zip(encoded.run_index[:-1],encoded.run_index[1:],encoded.run_original_id,strict=True):
        if original not in (identifier,identifier+1) or start%3 or stop%3 or not 0<=start<=stop<=3*len(triangles):
            raise ValueError('Invalid Boolean run provenance')
        owners[start//3:stop//3]=original-identifier
    if np.any(owners<0) or len(references)!=len(triangles):raise ValueError('Incomplete Boolean source relation')
    mesh=dict(vertices=properties[:,:3].copy()+anchor,faces=triangles.copy())
    for channel in range(10):mesh['uv'+str(channel) if channel else 'uv']=properties[:,3+2*channel:5+2*channel].copy()
    for owner in range(2):
        selected=owners==owner
        if np.any(references[selected]<0) or np.any(references[selected]>=len(sources[owner].mesh['faces'])):
            raise ValueError('Boolean relation left source triangles')
    return mesh,owners,references,vertex_roots(encoded,len(properties))


def boolean(a,b,operation='union',max_triangles=262144):
    if operation not in ('union','subtract','intersect'):raise ValueError('Unknown Boolean operation')
    if type(max_triangles)is not int or max_triangles<=0:raise ValueError('Invalid Boolean capacity')
    common=a.scope.object_frame
    anchor=np.asarray(local_mesh(a,common)['vertices'][0],dtype=float)
    identifier=manifold.Manifold.reserve_ids(2)
    sources=tuple(encoded_source(shape,identifier+n,anchor,max_triangles,common) for n,shape in enumerate((a,b)))
    mode=dict(union=manifold.OpType.Add,subtract=manifold.OpType.Subtract,intersect=manifold.OpType.Intersect)[operation]
    solid=manifold.Manifold.batch_boolean([s.solid for s in sources],mode)
    if solid.status()!=manifold.Error.NoError:raise ValueError('Boolean result rejected: '+str(solid.status()))
    if solid.num_tri()>max_triangles:raise ValueError('Boolean output triangle capacity exceeded')
    kept=[];contacts=[];volume=area=0.;offset=0
    for component in solid.decompose():
        component_volume=float(component.volume());component_area=float(component.surface_area())
        if component_volume==0.:
            contacts.append(dict(volume_m3=0.,area_m2=component_area,reason='Nonvolumetric closed-solid contact'))
            continue
        mesh,owners,references,roots=decode(component,sources,identifier,anchor,max_triangles)
        mesh['faces']+=offset;roots+=offset;offset+=len(mesh['vertices'])
        kept.append((mesh,owners,references,roots));volume+=component_volume;area+=component_area
    if not kept:
        return BooleanProduct(dict(vertices=np.empty((0,3)),faces=np.empty((0,3),dtype=np.int32)),
            np.empty(0,dtype=np.int32),np.empty(0,dtype=np.int32),sources,np.empty(0,dtype=np.int32),tuple(contacts),common,0.,0.)
    mesh={key:np.concatenate([row[0][key] for row in kept]) for key in kept[0][0]}
    return BooleanProduct(mesh,np.concatenate([r[1] for r in kept]),np.concatenate([r[2] for r in kept]),
        sources,np.concatenate([r[3] for r in kept]),tuple(contacts),common,volume,area)
