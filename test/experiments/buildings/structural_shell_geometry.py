"""Actual compiled shell sections and Native ray queries of the rendered assembly."""

import manifold3d as manifold
import numpy as np
import shapely

from building_vertical import mesh as complete_mesh
from opening_details import slots,expand
from outshine_ref.buildings.modelling.mesh_fans import connected_fans


def native(mesh,roots=None,rows=None):
    rows=np.arange(len(mesh['faces'])) if rows is None else np.asarray(rows)
    if roots is None:
        values=dict(mesh,uv=np.zeros((len(mesh['vertices']),2)))
        properties,triangles,duplicates,targets=connected_fans(values,rows)
        encoded=manifold.Mesh64(np.ascontiguousarray(properties[:,:3]),triangles,merge_from_vert=duplicates,merge_to_vert=targets)
    else:
        _,first,inverse=np.unique(roots,return_index=True,return_inverse=True)
        target=first[inverse];indices=np.arange(len(roots));selected=indices!=target
        encoded=manifold.Mesh64(np.array(mesh['vertices'],copy=True,order='C'),np.asarray(mesh['faces'],dtype=np.uint64),
            merge_from_vert=indices[selected].astype(np.uint64),merge_to_vert=target[selected].astype(np.uint64))
    solid=manifold.Manifold(encoded)
    if solid.status()!=manifold.Error.NoError:raise ValueError('Actual structural mesh rejected: '+str(solid.status()))
    return solid


def mesh_section(mesh,roots,height):
    points=mesh['vertices'];segments={};coordinates={}
    for triangle in mesh['faces']:
        xyz=points[triangle];distance=xyz[:,2]-height
        if np.all(distance>=0) or np.all(distance<=0):continue
        crossings=[]
        for a,b in ((0,1),(1,2),(2,0)):
            if distance[a]==0:
                key=('vertex',int(roots[triangle[a]]));coordinates[key]=xyz[a,:2];crossings.append(key)
            elif distance[a]*distance[b]<0:
                first,last=sorted((int(triangle[a]),int(triangle[b])),key=lambda i:int(roots[i]))
                key=('edge',*sorted((int(roots[first]),int(roots[last]))))
                p,q=points[first],points[last];coordinates[key]=(p+(q-p)*(height-p[2])/(q[2]-p[2]))[:2];crossings.append(key)
        crossings=list(dict.fromkeys(crossings))
        if len(crossings)!=2:raise ValueError('Structural section needs two stable edge intersections')
        a,b=crossings;normal=np.cross(xyz[1]-xyz[0],xyz[2]-xyz[0]);tangent=np.array([-normal[1],normal[0]])
        if (coordinates[b]-coordinates[a])@tangent<0:a,b=b,a
        if a in segments:raise ValueError('Structural section has a branching directed boundary')
        segments[a]=b
    polygons=[];signs=[]
    while segments:
        first=next(iter(segments));current=first;ring=[]
        while True:
            ring.append(coordinates[current])
            if current not in segments:raise ValueError('Structural section has an open native edge chain')
            current=segments.pop(current)
            if current==first:break
        xy=np.asarray(ring);local=xy-xy[0];area=np.sum(local[:,0]*np.roll(local[:,1],-1)-local[:,1]*np.roll(local[:,0],-1))/2
        polygon=shapely.Polygon(xy)
        if not polygon.is_valid:raise ValueError('Structural section recovered an invalid native ring')
        polygons.append(polygon);signs.append(area>0)
    result=shapely.Polygon()
    for polygon,positive in sorted(zip(polygons,signs,strict=True),key=lambda item:-item[0].area):
        result=result.union(polygon) if positive else result.difference(polygon)
    return result


def assembly(plan,shell,frames=False,omit_materials=()):
    detail=complete_mesh(plan);selected=~np.isin(detail['materials'],[0,1,2,*omit_materials])
    body=shell.mesh;offset=len(body['vertices'])
    product=dict(vertices=np.r_[body['vertices'],detail['vertices']],faces=np.r_[body['faces'],detail['faces'][selected]+offset],
        materials=np.r_[body['materials'],detail['materials'][selected]],parts=np.r_[np.zeros(len(body['faces']),dtype=np.int32),detail['parts'][selected]+1])
    solids=[native(body,shell.vertex_roots)]
    for part in np.unique(detail['parts'][selected]):
        solids.append(native(detail,rows=np.flatnonzero(selected&(detail['parts']==part))))
    if frames:
        framing=expand(slots(plan));offset=len(product['vertices']);part_offset=int(product['parts'].max())+1
        product=dict(vertices=np.r_[product['vertices'],framing['vertices']],faces=np.r_[product['faces'],framing['faces']+offset],
            materials=np.r_[product['materials'],framing['materials']],parts=np.r_[product['parts'],framing['parts']+part_offset])
        for part in np.unique(framing['parts']):solids.append(native(framing,rows=np.flatnonzero(framing['parts']==part)))
    collision=manifold.Manifold.batch_boolean(solids,manifold.OpType.Add)
    if collision.status()!=manifold.Error.NoError:raise ValueError('Rendered structural assembly is not a valid collision solid')
    return product,collision


class NativeColumns:
    def __init__(self,solid,low,high):self.solid,self.low,self.high=solid,low,high

    def intervals(self,point):
        hits=self.solid.ray_cast([*point,self.low],[*point,self.high]);events=[]
        for hit in hits:
            if abs(hit.normal[2])<1e-10:continue
            z=float(hit.position[2]);sign=-1 if hit.normal[2]>0 else 1
            if events and abs(z-events[-1][0])<1e-8 and sign==events[-1][1]:continue
            events.append((z,sign))
        intervals=[];winding=0;begin=None
        for height,sign in events:
            before=winding;winding+=sign
            if before==0 and winding>0:begin=height
            if before>0 and winding==0:intervals.append((begin,height))
            if winding<0:raise ValueError('Ray crossed an inconsistent structural boundary')
        if winding:raise ValueError('Ray did not leave the complete structural solid')
        return intervals


def inspect_poses(columns,poses,feet=True):
    angles=np.arange(16)*2*np.pi/16;ring=np.r_[[[0.,0.]],np.c_[np.cos(angles),np.sin(angles)]];minimum=float('inf')
    for pose in poses:
        for xy in pose[:2]+ring*.30:
            intervals=columns.intervals(xy)
            if any(high>pose[2]+.40+1e-6 and low<pose[2]+1.90-1e-6 for low,high in intervals):
                raise ValueError('Compiled structural mesh obstructs the 0.30m-radius/1.90m-height torso')
            minimum=min(minimum,min((low-pose[2] for low,_ in intervals if low>=pose[2]+.40),default=float('inf')))
        if feet:
            for xy in pose[:2]+ring*.10:
                intervals=columns.intervals(xy);top=max((high for _,high in intervals if high<=pose[2]+1e-6),default=-np.inf)
                if abs(top-pose[2])>1e-6 or any(low<pose[2]-1e-6 and high>pose[2]+1e-6 for low,high in intervals):
                    raise ValueError('Compiled structural mesh does not support the sampled foot disk')
    return minimum
