"""Edge-oriented planar shed roofs with holes, corner UVs and source appearance ownership."""

from dataclasses import dataclass,replace
import math

import numpy as np

from modelling_extrude import aligned_scope
from modelling_faces import Face,ROOF_TAGS,face_shape,plane
from modelling_metadata import inherit
from modelling_scope import scalar
from modelling_uv import corner_values


@dataclass(frozen=True)
class RoofInfo:
    selected_edges: tuple
    runs_m: tuple
    rises_m: tuple
    source_faces: tuple
    omitted_edges: tuple
    authored_normals_rebuilt: bool


def parameters(value,index,mode):
    value,index=scalar(value),scalar(index)
    if mode not in ('byAngle','byHeight'):raise ValueError('Unknown shed roof value type')
    if not -(2**31)<=index<2**31:raise ValueError('Roof edge index exceeds the signed 32-bit reference range')
    if mode=='byAngle' and not 1e-10<=abs(value)<90.:raise ValueError('Roof angle must have magnitude in [1e-10,90) degrees')
    if mode=='byHeight' and value<=0.:raise ValueError('Roof height must be positive')
    return value,int(round(index))


def channel_faces(face,fields):
    channels={0:corner_values(face,0,fields.get('uv'))}
    for key,stream in fields.items():
        if key.startswith('uv') and key[2:].isdigit():channels[int(key[2:])]=corner_values(face,int(key[2:]),stream)
    return channels


def cap(face,mapping,channels,tag,reverse=False):
    rings=[tuple(mapping[index] for index in ring) for ring in (face.outer,*face.holes)]
    values={channel:list(rows) for channel,rows in channels.items()}
    if reverse:
        rings=[ring[::-1] for ring in rings];values={channel:[row[::-1] for row in rows] for channel,rows in values.items()}
    return replace(face,outer=rings[0],holes=tuple(rings[1:]),tags=face.tags|{tag},
        uvs=tuple(values[0]),uv_sets=tuple((channel,tuple(rows)) for channel,rows in values.items() if channel))


def sides(face,mapping,channels,owner,omitted):
    result=[]
    for number,ring in enumerate((face.outer,*face.holes)):
        for corner,(a,b) in enumerate(zip(ring,ring[1:]+ring[:1])):
            raw=(a,b,mapping[b],mapping[a]);kept=[i for i in range(4) if raw[i]!=raw[i-1]]
            if len(kept)<3:
                omitted.append((owner,number,corner,'zero-height eave edge'));continue
            uv={channel:tuple((rows[number][corner],rows[number][(corner+1)%len(ring)],
                rows[number][(corner+1)%len(ring)],rows[number][corner])[i] for i in kept) for channel,rows in channels.items()}
            tag='roof.side.outer' if number==0 else 'roof.side.inner'
            result.append(Face(tuple(raw[i] for i in kept),material=face.material,tags=face.tags|{tag},
                uvs=(uv[0],),group=face.group,uv_sets=tuple((channel,(values,)) for channel,values in uv.items() if channel)))
    return result


def sweep_face(vertices,face,value,index,mode,fields,points,known,owner,omitted):
    channels=channel_faces(face,fields)
    face=replace(face,uvs=channels[0],uv_sets=tuple((channel,rows) for channel,rows in channels.items() if channel))
    normal,_,_,face=plane(vertices,face);face=replace(face,tags=face.tags-ROOF_TAGS.keys())
    selected=index%len(face.outer);a,b=face.outer[selected],face.outer[(selected+1)%len(face.outer)]
    tangent=vertices[b]-vertices[a];length=np.linalg.norm(tangent)
    if length<=1e-25:raise ValueError('Selected roof edge has no usable direction')
    direction=np.cross(normal,tangent/length)
    if value<0.:direction=-direction
    used=sorted({i for ring in (face.outer,*face.holes) for i in ring})
    positions=vertices[used];distances=(positions-vertices[a])@direction
    run=float(np.ptp(distances))
    if run<1.000000013351432e-10:raise ValueError('Roof has no usable run perpendicular to its selected edge')
    rise=value if mode=='byHeight' else run*math.tan(math.radians(abs(value)))
    fractions=(distances-distances.min())/run;fractions[fractions<=1e-12]=0.
    heights=fractions*rise;mapping={}
    for original,position,height in zip(used,positions,heights,strict=True):
        if height==0.:mapping[original]=original;continue
        target=position+normal*height;key=(original,*target)
        if key not in known:known[key]=len(points);points.append(target.tolist())
        mapping[original]=known[key]
    channels=channel_faces(face,fields);bottom={index:index for index in used}
    result=[cap(face,bottom,channels,'roof.bottom',True),cap(face,mapping,channels,'roof.top')]
    result+=sides(face,mapping,channels,owner,omitted)
    return result,normal,selected,run,rise


def roof_shed(shape,value,index=0,mode='byAngle'):
    value,index=parameters(value,index,mode);geometry=shape.geometry
    if geometry.edges or geometry.points or not geometry.polygons:
        raise NotImplementedError('Shed roofs require logical polygon surfaces without wire or point primitives')
    allowed={'faces','parts','materials','groups','face_tag_mask','normals','uv'}|{'uv'+str(i) for i in range(1,10)}
    if set(geometry.fields)-allowed:raise NotImplementedError('Shed roof vertex fields other than normals and UV0–9 remain pending')
    mesh=shape.emit();vertices=mesh['vertices'];points=vertices.tolist();faces=[];owners=[];normals=[];selected=[];runs=[];rises=[];omitted=[];known={}
    for number,face in enumerate(geometry.polygons):
        output,normal,edge,run,rise=sweep_face(vertices,face,value,index,mode,mesh,points,known,number,omitted)
        faces+=output;owners.extend([number]*len(output));normals.append(normal);selected.append(edge);runs.append(run);rises.append(rise)
    vertices=np.asarray(points);used=sorted({i for face in faces for ring in (face.outer,*face.holes) for i in ring})
    scope=aligned_scope(vertices[used],normals[0],shape.scope,False)
    result=inherit(face_shape(vertices,faces,scope=scope),shape)
    if 'uv' not in geometry.fields:result=result.delete_uv(0)
    return replace(result,comp_info=shape.comp_info,
        roof_info=RoofInfo(tuple(selected),tuple(runs),tuple(rises),tuple(owners),tuple(omitted),'normals' in geometry.fields))
