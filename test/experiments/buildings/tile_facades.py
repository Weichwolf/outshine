"""Context-scoped compact facade plans on existing complete source envelopes."""

from collections import Counter
import hashlib
import math
import time

import mapbox_vector_tile
import numpy as np
import shapely

from context import attach
from recipes import choose
from samples import SOURCES
from tile_roof_envelopes import OrdinaryDetails
from outshine_ref.buildings.facades import FacadeRepeat


def contexts(rows,receipt):
    r=receipt['receipt'];path=SOURCES/(hashlib.sha256(r['url'].encode()).hexdigest()+'.pbf');data=path.read_bytes()
    if hashlib.sha256(data).hexdigest()!=r['sha256']:raise ValueError('Facade context provider bytes changed')
    layers=mapbox_vector_tile.decode(data,default_options={'y_coord_down':True});span=receipt['span_m']
    bodies=[r for r in rows if r['layer']=='building']
    def project(points,extent):return (points/extent-.5)*[span,-span]
    return attach(bodies,layers,project,r['sha256'])


def project(mesh,row,parameters,fractions,axis_m,floor_m):
    triangles=mesh['vertices'][mesh['faces']];vectors=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0])
    normals=vectors/np.linalg.norm(vectors,axis=1,keepdims=True);walls=abs(normals[:,2])<1e-8
    top=float(parameters.get('eave_m',parameters['buildingHeight']));bottom=parameters['minHeight'];height=top-bottom
    rows=max(1,int(math.floor(height/floor_m+.5)));uv=np.zeros((len(triangles),3,2));extent=np.zeros((len(triangles),3,3));assigned=np.zeros(len(triangles),dtype=bool);facades=[]
    polygon=shapely.orient_polygons(row['polygon'])
    for ring in (polygon.exterior,*polygon.interiors):
        for a,b in zip(list(ring.coords)[:-1],list(ring.coords)[1:],strict=True):
            a,b=np.asarray(a),np.asarray(b);delta=b-a;length=float(np.linalg.norm(delta))
            if length==0:continue
            tangent=delta/length;normal=np.array([tangent[1],-tangent[0]])
            distance=(triangles[:,:,:2]-a)@normal;along=(triangles[:,:,:2]-a)@tangent
            selected=walls&((normals[:,:2]@normal)>.999999)&(abs(distance).max(axis=1)<1e-7)&(along.min(axis=1)>=-1e-7)&(along.max(axis=1)<=length+1e-7)
            if not selected.any():continue
            if np.any(assigned&selected):raise ValueError('Facade triangle has multiple source boundary owners')
            columns=max(1,int(math.floor(length/axis_m+.5)));repeat=FacadeRepeat(length,height,columns,rows,*fractions)
            uv[selected,:,0]=along[selected]/repeat.cell_size_m[0]
            uv[selected,:,1]=(triangles[selected,:,2]-bottom)/repeat.cell_size_m[1]
            extent[selected]=[columns,rows,0];assigned|=selected
            facades.append(dict(id=row['id']+f'/facade-{len(facades)}',a=a.tolist(),b=b.tolist(),width_m=length,height_m=height,
                columns=columns,rows=rows,fractions=list(fractions),opening_count=repeat.opening_count))
    if np.any(walls&~assigned):raise ValueError('Exterior wall lacks a source facade frame')
    if not assigned.any():raise ValueError('No positive exterior facade in complete source envelope')
    result={key:value.copy() for key,value in mesh.items() if key not in ('vertices','faces','uv','normals')}
    result.update(vertices=triangles.reshape(-1,3),faces=np.arange(triangles.size//3,dtype=np.uint32).reshape(-1,3),
        normals=mesh['normals'][mesh['faces']].reshape(-1,3),uv=uv.reshape(-1,2),facade_extent=extent.reshape(-1,3))
    if not np.array_equal(result['vertices'][result['faces']],triangles):raise ValueError('Facade binding changed original source triangles')
    return result,assigned,facades


class FacadeDetails:
    def __init__(self,rows,view):
        self.base=OrdinaryDetails(rows,view);self.palette={};self.bindings={};self.completed=[];self.rejected=[]

    def __call__(self,row,parameters,material):
        before=self.base(row,parameters,material)
        if before is None:
            from tile_buildings import prism
            before=prism(row,parameters,material)
        mesh,info=before
        if 'room_ids' in info:return before
        began=time.perf_counter();recipe=choose(row['properties'],row.get('context',{}),row['polygon'],parameters['buildingHeight'],parameters['minHeight'])
        family=recipe['family'];rhythm=recipe['parameters']
        if family in ('unknown','utility','garage'):return before
        try:
            fractions=(rhythm['width'],rhythm['height'])
            projected,selected,facades=project(mesh,row,parameters|info,fractions,rhythm['axis_m'],rhythm['floor_m'])
            key=(material,*fractions)
            if key not in self.palette:
                identifier=32768+len(self.palette);self.palette[key]=identifier;self.bindings[str(identifier)]=dict(wall_material=material,fractions=list(fractions),filtered=True)
            projected['materials'][selected]=self.palette[key]
            record=dict(id=row['id'],family=family,origin=recipe['origin'],reason=recipe['reason'],facades=facades,
                source_properties=row['properties'],site_use=recipe['site_use'],build_ms=(time.perf_counter()-began)*1000,
                scope='Procedural opening/material estimate on complete exterior; full near room/door programme not admitted')
            self.completed.append(record)
            return projected,info|dict(facade_recipe=family,facade_origin=recipe['origin'],facade_plan=facades,
                functional_status='Exterior facade placeholder, source use unproven; actual rooms remain separate unresolved programme')
        except ValueError as error:
            self.rejected.append(dict(id=row['id'],reason=str(error)));return before

    def report(self):return dict(completed=self.completed,rejected=self.rejected,families=dict(Counter(r['family'] for r in self.completed)),
        scope='Context-scoped exterior placeholders, zero completed architecture families; existing constructed room controls untouched')
