"""Bind complete native triangle subsets to Mitsuba while checking exported geometry."""

import numpy as np
import mitsuba as mi


def mesh(name,data,rows,bsdf,emitter=None):
    indices=data['faces'][rows];used=np.unique(indices);positions=data['vertices'][used].astype(np.float32)
    faces=np.searchsorted(used,indices).astype(np.uint32)
    props=mi.Properties();props['bsdf']=mi.load_dict(bsdf)
    if emitter is not None:props['emitter']=mi.load_dict(emitter)
    uv=None
    if 'uv' in data:
        source_uv=np.asarray(data['uv'])
        if source_uv.shape!=(len(data['vertices']),2) or not np.all(np.isfinite(source_uv)):
            raise ValueError('Physical mesh requires finite per-vertex UV pairs')
        uv=source_uv[used].astype(np.float32)
    normals=None
    if 'normals' in data:
        source_normals=np.asarray(data['normals'])
        if source_normals.shape!=(len(data['vertices']),3) or not np.isfinite(source_normals).all() or not np.allclose(np.linalg.norm(source_normals,axis=1),1.,atol=1e-6,rtol=0):
            raise ValueError('Physical mesh requires finite unit per-vertex normals')
        normals=source_normals[used].astype(np.float32)
    result=mi.Mesh(name,vertex_count=len(positions),face_count=len(faces),props=props,has_vertex_texcoords=uv is not None,has_vertex_normals=normals is not None)
    values=mi.traverse(result)
    values['vertex_positions']=mi.Float(positions.ravel());values['faces']=mi.UInt32(faces.ravel())
    if uv is not None:values['vertex_texcoords']=mi.Float(uv.ravel())
    if normals is not None:values['vertex_normals']=mi.Float(normals.ravel())
    values.update()
    if not np.array_equal(np.asarray(values['vertex_positions']).reshape(-1,3),positions) or not np.array_equal(np.asarray(values['faces']).reshape(-1,3),faces):
        raise ValueError('Mitsuba geometry binding differs from the source triangles')
    if uv is not None and not np.array_equal(np.asarray(values['vertex_texcoords']).reshape(-1,2),uv):
        raise ValueError('Mitsuba UV binding differs from the source coordinates')
    if normals is not None and not np.array_equal(np.asarray(values['vertex_normals']).reshape(-1,3),normals):
        raise ValueError('Mitsuba normal binding differs from the source coordinates')
    return result,dict(triangles=len(faces),vertices=len(used),max_position_rounding_m=float(np.max(abs(positions-data['vertices'][used]))) if len(used) else 0.)
