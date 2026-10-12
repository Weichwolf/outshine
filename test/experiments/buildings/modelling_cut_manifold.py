"""Closed-solid plane-cut candidate using Manifold while retaining source triangle properties."""

from dataclasses import dataclass

import manifold3d as manifold
import numpy as np

from mesh_fans import connected_fans
from modelling_component_geometry import topology
from modelling_cut_topology import logical_faces,vertex_roots
from modelling_faces import Face,EXTRUSION_TAGS


@dataclass
class CutSource:
    shape: object
    solid: object
    original_id: int
    polygons: tuple
    triangle_parts: object
    authored_normals: bool
    properties: object
    triangles: object
    roots: object


def encoded_source(shape):
    if shape.geometry.edges or shape.geometry.points:raise NotImplementedError('Mixed wire/surface plane cuts remain pending')
    mesh=shape.emit();allowed={'vertices','faces','uv','normals','parts','materials','groups','face_tag_mask'}
    if set(mesh)-allowed:raise ValueError('Plane cut has no inheritance contract for '+str(sorted(set(mesh)-allowed)))
    if not len(mesh['faces']):raise ValueError('Closed-solid cut needs nonempty triangle geometry')
    if not shape.geometry.polygons and 'parts' in mesh:raise NotImplementedError('Raw mesh part provenance without logical faces remains pending')
    polygons,_=topology(shape,mesh);parts=mesh.get('parts',np.arange(len(mesh['faces'])))
    mesh=dict(mesh);mesh.setdefault('uv',np.zeros((len(mesh['vertices']),2)))
    triangles=(mesh['vertices']-mesh['vertices'].mean(axis=0))[mesh['faces']]
    signed_volume=np.einsum('ij,ij->i',triangles[:,0],np.cross(triangles[:,1],triangles[:,2])).sum()/6
    if signed_volume<0:mesh['faces']=mesh['faces'][:,::-1]
    order=np.arange(len(mesh['faces']));properties,triangles,duplicates,targets=connected_fans(mesh,order)
    if 'normals' in mesh:properties=np.ascontiguousarray(np.c_[properties,mesh['normals'][mesh['faces']].reshape(-1,3)])
    identifier=manifold.Manifold.reserve_ids(1)
    encoded=manifold.Mesh64(properties,triangles,merge_from_vert=duplicates,merge_to_vert=targets,
        run_index=np.asarray([0,len(triangles)*3],dtype=np.uint64),run_original_id=np.asarray([identifier],dtype=np.uint32),
        face_id=np.arange(len(triangles),dtype=np.uint64))
    solid=manifold.Manifold(encoded)
    if solid.status()!=manifold.Error.NoError or solid.is_empty():raise ValueError('Closed-solid plane cut rejected source: '+str(solid.status()))
    roots=np.arange(len(properties));roots[duplicates]=targets
    return CutSource(shape,solid,identifier,polygons,parts,'normals' in mesh,properties,triangles,roots)


def volumetric_components(source,solid):
    kept=[];omitted=[]
    for component in solid.decompose():
        volume=component.volume()
        if volume>0:kept.append(component);continue
        encoded=component.to_mesh64();owners=set()
        for start,stop,identifier in zip(encoded.run_index[:-1],encoded.run_index[1:],encoded.run_original_id,strict=True):
            if identifier==source.original_id:
                owners.update(int(source.triangle_parts[face]) for face in encoded.face_id[start//3:stop//3])
        omitted.append((tuple(sorted(owners)),float(volume),float(component.surface_area()),'zero-volume cut contact'))
    if not omitted:return solid,()
    return (manifold.Manifold.batch_boolean(kept,manifold.OpType.Add) if kept else manifold.Manifold()),tuple(omitted)


def decode_cut(source,solid,axis,cut_material=None,cut_group=0):
    if solid.status()!=manifold.Error.NoError:raise ValueError('Plane cut failed: '+str(solid.status()))
    encoded=solid.to_mesh64();properties=np.asarray(encoded.vert_properties)
    triangles=np.asarray(encoded.tri_verts,dtype=np.int32)
    if not len(triangles):return None
    identifiers=np.empty(len(triangles),dtype=np.uint32)
    for start,stop,identifier in zip(encoded.run_index[:-1],encoded.run_index[1:],encoded.run_original_id,strict=True):
        identifiers[start//3:stop//3]=identifier
    face_ids=np.asarray(encoded.face_id,dtype=np.int64);keys=[];templates={}
    source_materials={face.material for face in source.polygons}
    if cut_material is None and np.any(identifiers!=source.original_id):
        if len(source_materials)!=1:raise ValueError('A multi-material source needs an explicit cut material')
        cut_material=next(iter(source_materials))
    corners=triangles.ravel();roots=vertex_roots(encoded,len(properties))[corners]
    properties=properties[corners];vertices=properties[:,:3].copy();uv=properties[:,3:5].copy()
    triangles=np.arange(len(corners),dtype=np.int32).reshape(-1,3)
    for row,(identifier,face_id) in enumerate(zip(identifiers,face_ids,strict=True)):
        if identifier==source.original_id:
            index=int(source.triangle_parts[face_id]);key=('source',index);templates[key]=(source.polygons[index],index)
        else:
            key=('cut',int(identifier),int(face_id));templates[key]=(Face((0,1,2),material=cut_material,group=cut_group,tags=frozenset({'split.cut'})),None)
            local=(vertices[triangles[row]]-source.shape.scope.origin)@source.shape.scope.axes
            uv[triangles[row]]=local[:,[dimension for dimension in range(3) if dimension!=axis]]
        keys.append(key)
    mesh=dict(vertices=vertices,faces=triangles,uv=uv)
    polygons,source_indices=logical_faces(mesh,roots,keys,templates)
    mesh['materials']=np.asarray([polygons[index].material for index in mesh['parts']],dtype=np.uint16)
    mesh['groups']=np.asarray([polygons[index].group for index in mesh['parts']],dtype=np.uint32)
    mesh['face_tag_mask']=np.asarray([sum(bit for name,bit in EXTRUSION_TAGS.items() if name in polygons[index].tags) for index in mesh['parts']],dtype=np.uint8)
    if source.authored_normals:
        normals=properties[:,5:8].copy()
        for row,key in enumerate(keys):
            if key[0]=='cut':
                points=vertices[triangles[row]];normal=np.cross(points[1]-points[0],points[2]-points[0]);normal/=np.linalg.norm(normal)
                normals[triangles[row]]=normal
        lengths=np.linalg.norm(normals,axis=1);mesh['normals']=np.divide(normals,lengths[:,None],out=np.zeros_like(normals),where=lengths[:,None]>0)
    return mesh,polygons,source_indices
