"""Bind complete native triangle subsets to Mitsuba while checking exported geometry."""

import numpy as np
import mitsuba as mi


def mesh(name,data,rows,bsdf,emitter=None):
    indices=data['faces'][rows];used=np.unique(indices);positions=data['vertices'][used].astype(np.float32)
    faces=np.searchsorted(used,indices).astype(np.uint32)
    props=mi.Properties();props['bsdf']=mi.load_dict(bsdf)
    if emitter is not None:props['emitter']=mi.load_dict(emitter)
    result=mi.Mesh(name,vertex_count=len(positions),face_count=len(faces),props=props)
    values=mi.traverse(result)
    values['vertex_positions']=mi.Float(positions.ravel());values['faces']=mi.UInt32(faces.ravel());values.update()
    if not np.array_equal(np.asarray(values['vertex_positions']).reshape(-1,3),positions) or not np.array_equal(np.asarray(values['faces']).reshape(-1,3),faces):
        raise ValueError('Mitsuba geometry binding differs from the source triangles')
    return result,dict(triangles=len(faces),vertices=len(used),max_position_rounding_m=float(np.max(abs(positions-data['vertices'][used]))) if len(used) else 0.)
