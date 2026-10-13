"""Two-plane roofs along projected scope X, preserving source triangle UVs at every cut."""

from dataclasses import dataclass,replace
import math

import numpy as np

from .mesh_fans import connected_fans
from .cut_geos import Slab
from .cut_source import CutInput
from .extrude import aligned_scope
from .faces import ROOF_TAGS,plane
from .metadata import inherit
from .roof_mesh import RoofMesh
from .roof_shed import channel_faces
from .scope import scalar
from .shape import Shape
from .uv import name


@dataclass(frozen=True)
class RidgeInfo:
    runs_m: tuple
    rises_m: tuple
    ridge_directions: tuple
    source_faces: tuple
    omitted_edges: tuple
    authored_normals_rebuilt: bool


def source_cut(shape,mesh,number,face,normal,channels):
    rows=np.flatnonzero(mesh['parts']==number)
    if not len(rows):raise ValueError('Ridge roof source face has no native triangles')
    points=mesh['vertices'][mesh['faces'][rows]];vectors=np.cross(points[:,1]-points[:,0],points[:,2]-points[:,0])
    if np.any(vectors@normal<=1e-12):raise ValueError('Ridge source triangles disagree with their logical face orientation')
    _,triangles,duplicates,targets=connected_fans(mesh,rows)
    corners=mesh['faces'][rows].ravel();properties=np.c_[mesh['vertices'][corners],*[mesh[name(channel)][corners] for channel in channels]]
    roots=np.arange(len(properties));roots[duplicates]=targets
    polygons=list(shape.geometry.polygons);polygons[number]=replace(face,uv_sets=(),uvs=())
    return CutInput(shape,tuple(polygons),np.full(len(rows),number),False,properties,triangles,roots,False,np.tile(normal,(len(rows),1)))


def edge_records(vertices,face):
    records=[]
    for number,ring in enumerate((face.outer,*face.holes)):
        records.extend((number,index,vertices[a],vertices[b]) for index,(a,b) in enumerate(zip(ring,ring[1:]+ring[:1])))
    if any(np.linalg.norm(row[3]-row[2])<=1e-25 for row in records):raise ValueError('Ridge roof input has a zero-length boundary edge')
    return records


def edge_owner(point,records):
    a=np.asarray([row[2] for row in records]);delta=np.asarray([row[3]-row[2] for row in records])
    length=np.einsum('ij,ij->i',delta,delta);fraction=np.clip(np.einsum('ij,ij->i',point-a,delta)/length,0.,1.)
    distance=np.linalg.norm(a+fraction[:,None]*delta-point,axis=1);index=int(np.argmin(distance))
    if distance[index]>1e-7:raise ValueError('Roof side lost its source boundary edge')
    return records[index][:2]


def heights(points,origin,direction,low,high,rise):
    coordinates=(points-origin)@direction;fraction=1.-np.abs((coordinates-(low+high)/2.)/((high-low)/2.))
    fraction=np.clip(fraction,0.,1.);fraction[fraction<=1e-12]=0.;fraction[fraction>=1.-1e-12]=1.
    return fraction*rise


def sides(builder,vertices,face,normal,origin,direction,low,high,rise,omitted):
    mesh,roots,bottom=builder.bottom_faces();records=edge_records(vertices,face)
    for boundary in bottom:
        for ring in (boundary.outer,*boundary.holes):
            ring=ring[::-1]
            for a,b in zip(ring,ring[1:]+ring[:1]):
                base=mesh['vertices'][[a,b]];height=heights(base,origin,direction,low,high,rise)
                owner=edge_owner(base.mean(axis=0),records)
                if not np.any(height):omitted.add((*owner,'zero-height eave edge'));continue
                values=np.c_[base,*[mesh[name(channel)][[a,b]] for channel in builder.channels]]
                upper=values.copy();upper[:,:3]+=height[:,None]*normal
                identities=[int(roots[a]),int(roots[b])];top=[identities[i] if height[i]==0. else builder.root(('top',identities[i])) for i in range(2)]
                raw=identities+top[::-1];kept=[i for i in range(4) if raw[i]!=raw[i-1]]
                properties=np.r_[values,upper[::-1]];tag='roof.side.outer' if owner[0]==0 else 'roof.side.inner'
                builder.polygon(properties[kept],[raw[i] for i in kept],('side',*owner),replace(face,tags=face.tags|{tag}))


def ridge_face(shape,mesh,number,value,mode,channels):
    face=shape.geometry.polygons[number];uv=channel_faces(face,mesh)
    face=replace(face,uvs=uv[0],uv_sets=tuple((channel,values) for channel,values in uv.items() if channel))
    normal,_,_,face=plane(mesh['vertices'],face);face=replace(face,tags=face.tags-ROOF_TAGS.keys())
    x=shape.scope.axes[:,0]-normal*np.dot(normal,shape.scope.axes[:,0])
    if np.linalg.norm(x)<1e-10:x=shape.scope.axes[:,1]-normal*np.dot(normal,shape.scope.axes[:,1])
    x/=np.linalg.norm(x);direction=np.cross(normal,x);origin=mesh['vertices'][face.outer[0]]
    used=[i for ring in (face.outer,*face.holes) for i in ring];coordinates=(mesh['vertices'][used]-origin)@direction
    low,high=float(coordinates.min()),float(coordinates.max());run=high-low
    if run<1.000000013351432e-10:raise ValueError('Roof has no usable run perpendicular to its ridge')
    rise=value if mode=='byHeight' else math.tan(math.radians(value))*run/2.
    source=source_cut(shape,mesh,number,face,normal,channels);builder=RoofMesh(channels);omitted=set()
    for half,(a,b) in enumerate(((low,(low+high)/2.),((low+high)/2.,high))):
        slab=Slab(source,2,a,b,None,0,direction,origin);slab.surfaces();properties=np.asarray(slab.properties);roots=builder.cut_roots(slab)
        bottom_order=np.arange(len(properties)).reshape(-1,3)[:,::-1].ravel()
        builder.append(properties[bottom_order],roots[bottom_order],'bottom',replace(face,tags=face.tags|{'roof.bottom'}))
        height=heights(properties[:,:3],origin,direction,low,high,rise);top=properties.copy();top[:,:3]+=height[:,None]*normal
        top_roots=[int(root) if h==0. else builder.root(('top',int(root))) for root,h in zip(roots,height,strict=True)]
        builder.append(top,top_roots,('top',half),replace(face,tags=face.tags|{'roof.top'}))
    sides(builder,mesh['vertices'],face,normal,origin,direction,low,high,rise,omitted)
    native,faces=builder.finish();return native,faces,normal,x,run,rise,tuple(sorted(omitted))


def roof_ridge(shape,value,mode='byAngle'):
    value=scalar(value)
    if mode not in ('byAngle','byHeight'):raise ValueError('Unknown ridge roof value type')
    if value<=0. or mode=='byAngle' and not 1e-10<=value<90.:raise ValueError('Ridge roofs need a positive height or angle in [1e-10,90) degrees')
    if shape.geometry.edges or shape.geometry.points or not shape.geometry.polygons:
        raise NotImplementedError('Ridge roofs require logical polygon surfaces without wire or point primitives')
    allowed={'faces','parts','materials','groups','face_tag_mask','normals','uv'}|{'uv'+str(i) for i in range(1,10)}
    if set(shape.geometry.fields)-allowed:raise NotImplementedError('Ridge roof fields other than normals and UV0–9 remain pending')
    mesh=shape.emit();mesh.setdefault('uv',np.zeros((len(mesh['vertices']),2)));channels=tuple([0]+sorted(int(k[2:]) for k in mesh if k.startswith('uv') and k[2:].isdigit()))
    products=[];faces=[];owners=[];runs=[];rises=[];directions=[];normals=[];omitted=[];vertex_offset=face_offset=0
    for number in range(len(shape.geometry.polygons)):
        native,polygons,normal,x,run,rise,missing=ridge_face(shape,mesh,number,value,mode,channels)
        native['faces']+=vertex_offset;native['parts']+=face_offset;products.append(native)
        faces.extend(replace(face,outer=tuple(i+vertex_offset for i in face.outer),holes=tuple(tuple(i+vertex_offset for i in ring) for ring in face.holes)) for face in polygons)
        owners.extend([number]*len(polygons));runs.append(run);rises.append(rise);directions.append(tuple(x));normals.append(normal)
        omitted.extend((number,*row) for row in missing);vertex_offset+=len(native['vertices']);face_offset+=len(polygons)
    native={key:np.concatenate([product[key] for product in products]) for key in products[0]}
    scope=aligned_scope(native['vertices'],normals[0],shape.scope,False);result=inherit(Shape.from_mesh(native,scope,faces),shape)
    if 'uv' not in shape.geometry.fields:result=result.delete_uv(0)
    return replace(result,comp_info=shape.comp_info,roof_info=RidgeInfo(tuple(runs),tuple(rises),tuple(directions),tuple(owners),tuple(omitted),'normals' in shape.geometry.fields))
