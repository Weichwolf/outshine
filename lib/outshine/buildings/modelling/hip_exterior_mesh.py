"""Lift a conformal collar along the original face normal, keeping native UV charts."""

from dataclasses import replace

import numpy as np

from .faces import Face
from .hip_exterior import prepare_hip_exterior
from .hip_exterior_overlay import overlay
from .hip_sides import close_rim
from .roof_mesh import RoofMesh
from .uv_exterior import extrapolate_chart


def shared_roots(builder,points,heights,patches,gradients,surface):
    roots=[];upper=[];known={};adjustment=0.
    for xy,height,owner in zip(points,heights,patches,strict=True):
        key=tuple(xy)
        uncertainty=np.sqrt(2)*surface.precision_m*np.linalg.norm(gradients[owner])+1e-7
        if height<surface.base_level_m-uncertainty:
            raise ValueError('Exterior roof top is below its bottom')
        if key in known:
            root,prior=known[key];difference=abs(height-prior)
            if difference>uncertainty:raise ValueError('Shared exterior height disagreement')
            adjustment=max(adjustment,difference)
        else:
            root=builder.root(('base',*key));known[key]=(root,float(height))
        roots.append(root);upper.append(builder.root(('top',root)))
    return np.asarray(roots),np.asarray(upper),np.asarray([known[tuple(p)][1] for p in points]),adjustment


def exterior_mesh(source,projected,plan,field,value,mode,overhang,channels,normal,origin,basis,
                  template,max_cells,shell_depth_m):
    surface=prepare_hip_exterior(plan,value,mode,overhang,max_cells,shell_depth_m)
    corners=projected[source.triangles];factor=field.factor(value,mode)
    cut,gradients,intercepts,_=overlay(corners,plan,field,surface,factor,max_cells)
    points=cut.corners.reshape(-1,2)
    triangles=np.repeat(cut.source_triangles,3);patches=np.repeat(cut.facets,3)
    properties=np.empty((len(points),source.properties.shape[1]))
    native=source.properties[source.triangles]
    for triangle in np.unique(triangles):
        selected=triangles==triangle
        properties[selected]=extrapolate_chart(corners,native,int(triangle),points[selected])
    heights=np.einsum('ij,ij->i',points,gradients[patches])+intercepts[patches]
    builder=RoofMesh(channels,normal)
    roots,upper,heights,adjustment=shared_roots(builder,points,heights,patches,gradients,surface)
    properties[:,:3]=origin+(points+plan.origin)@basis+surface.base_level_m*normal
    face=Face((0,1,2),material=template.material,tags=template.tags,group=template.group)
    order=np.arange(len(properties)).reshape(-1,3)[:,::-1].ravel()
    builder.append(properties[order],roots[order],'bottom',replace(face,tags=face.tags|{'roof.bottom'}))
    close_rim(builder,normal,dict(zip(roots,heights-surface.base_level_m,strict=True)),face)
    top=properties.copy();top[:,:3]+=(heights-surface.base_level_m)[:,None]*normal
    for patch in np.unique(patches):
        selected=patches==patch
        builder.append(top[selected],upper[selected],('top',int(patch)),replace(face,tags=face.tags|{'roof.top'}))
    mesh,faces=builder.finish()
    return mesh,faces,cut.cells,surface,adjustment
