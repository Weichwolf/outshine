"""Bounded near controls from complete generic room plans in an otherwise complete tile."""

import time

import numpy as np
import shapely

from boolean_building_bands import construct
from building_vertical import build
from provider_contract import attributes
from structural_shell_geometry import assembly
from verify_structural_shell import access
from outshine_ref.buildings.structures import classify_structure


def numeric_properties(row):
    return {k:v for k,v in row['properties'].items() if k not in ('colour','color','building:colour')}


def candidates(rows,view,limit=16):
    ranked=[]
    for row in rows:
        if row['layer']!='building' or row['clipped']:continue
        try:
            p=attributes(numeric_properties(row));polygon=row['polygon']
            if p['hide3d'] or p['minHeight']!=0. or not 3.5<=p['buildingHeight']<20.:continue
            if not 70<=polygon.area<=800:continue
            if sum(len(r.coords)-1 for r in [polygon.exterior,*polygon.interiors])>32:continue
            xy=np.asarray(polygon.minimum_rotated_rectangle.exterior.coords)
            if np.min(np.linalg.norm(np.diff(xy,axis=0),axis=1))<8:continue
            if classify_structure(polygon,row['properties'],coordinate_step_m=row['coordinate_step_m']).kind!='ordinary':continue
            x0,y0,x1,y1=polygon.bounds
            points=np.array([(x,y,z) for x in (x0,x1) for y in (y0,y1) for z in (0.,p['buildingHeight'])])
            projected=(points-view.eye)@view.basis.T
            if np.min(projected[:,2])<=0:continue
            uv=projected[:,:2]*view.focal_px/projected[:,2:3]+[view.width/2,view.height/2]
            low=np.maximum(uv.min(axis=0),0);high=np.minimum(uv.max(axis=0),[view.width,view.height])
            if np.any(high<=low) or 1.2*view.focal_px/np.max(projected[:,2])<3.:continue
            ranked.append((float(np.prod(high-low)),row['id']))
        except (ValueError,shapely.errors.GEOSException):continue
    chosen=sorted(ranked,key=lambda item:(-item[0],item[1]))[:limit]
    return [identifier for score,identifier in chosen],dict(eligible=len(ranked),selected=len(chosen),
        selection='Highest visible envelope pixel areas among unclipped ordinary 70–800m² ground-level bodies, width>=8m, <=32 corners, height3.5–20m and 1.2m opening>=3px',
        scope='At most sixteen complete near-control plans per tile; this is an experiment bound, not a native LOD or frame quota')


def corner_buffers(mesh,wall_material):
    faces=mesh['faces'];points=mesh['vertices'][faces]
    normals=np.cross(points[:,1]-points[:,0],points[:,2]-points[:,0]);length=np.linalg.norm(normals,axis=1)
    if not np.isfinite(points).all() or not np.isfinite(length).all() or np.any(length==0):
        raise ValueError('Complete room detail contains unresolved geometry')
    normals/=length[:,None];materials=mesh['materials'].astype(np.uint16).copy()
    materials[materials==1]=wall_material
    materials[(mesh['materials']==2)&(normals[:,2]>.5)]=9
    return dict(vertices=points.reshape(-1,3),faces=np.arange(points.size//3,dtype=np.uint32).reshape(-1,3),
        normals=np.repeat(normals,3,axis=0),uv=np.zeros((points.size//3,2)),materials=materials)


class RoomDetails:
    def __init__(self,rows,view,limit=16):
        identifiers,self.selection=candidates(rows,view,limit);self.identifiers=set(identifiers)
        self.completed=[];self.rejected=[]

    def __call__(self,row,parameters,material):
        if row['id'] not in self.identifiers:return None
        try:return self.complete(row,material)
        except (ValueError,shapely.errors.GEOSException) as error:
            self.rejected.append(dict(id=row['id'],reason=str(error)));return None

    def complete(self,row,material):
        start=time.perf_counter();sample=dict(id=row['id'],polygon=row['polygon'],properties=numeric_properties(row))
        plan=build(sample);plan_ms=(time.perf_counter()-start)*1000
        if plan.get('vertical_status')!='connected-storeys':
            self.rejected.append(dict(id=row['id'],reason='No complete vertical plan',omissions=plan['omissions']));return None
        start=time.perf_counter();shell=construct(plan);shell_ms=(time.perf_counter()-start)*1000
        start=time.perf_counter();mesh,collision=assembly(plan,shell,frames=True);assembly_ms=(time.perf_counter()-start)*1000
        proof=access(plan,collision)
        output=corner_buffers(mesh,material)
        info=dict(form='Complete generic room/storey/door/window/stair structure from source envelope',
            storeys=plan['storeys'],rooms=len(plan['rooms'])*plan['storeys'],seed_method=plan['circulation']['seed_method'],
            access_mode=plan['circulation_status'],
            routing_attempts=len(plan['circulation_alternatives']),room_ids=plan['storey_room_ids'],
            plan_ms=plan_ms,shell_ms=shell_ms,assembly_ms=assembly_ms,access=proof,
            functional_status='Sampled actual structural access; generic programme, external terrain/transport contact and native controller unproved')
        self.completed.append(dict(id=row['id'],**info));return output,info

    def report(self):
        unprocessed=sorted(self.identifiers-{row['id'] for row in self.completed}-{row['id'] for row in self.rejected})
        return self.selection|dict(completed=self.completed,rejected=self.rejected,unprocessed=unprocessed)
