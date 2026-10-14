"""Source-stable scalar roof choices and bounded exterior envelopes for small ordinary bodies."""

from collections import Counter
from dataclasses import dataclass
import math
import time

import numpy as np
import shapely
from shapely.affinity import translate

from building_layout import frame
from provider_contract import number
from outshine_ref.buildings.modelling import Shape,cga_to_enu,heightfield_envelope
from tile_buildings import prism
from tile_room_details import RoomDetails


@dataclass(frozen=True)
class RoofChoice:
    kind: str
    rise_m: float
    eave_m: float
    ridge_degrees: float
    origin: str
    reason: str
    edge_index: int=0


def choose(row,parameters):
    polygon=row['polygon'];top=parameters['buildingHeight'];height=parameters['bodyHeight']
    supplied=row['properties'].get('roof:shape',row['properties'].get('roof_shape'))
    if supplied is not None:
        requested={'flat':'flat','gabled':'ridge','gable':'ridge','ridge':'ridge','skillion':'shed','shed':'shed'}.get(str(supplied).lower())
        if requested!='flat':
            return RoofChoice('flat',0.,top,0.,'supplied unsupported in this tile prototype',
                'supplied roof retained in source attributes; explicit sloped-roof/room attachment remains outside this small-body estimate')
        return RoofChoice('flat',0.,top,0.,'supplied','explicit flat roof')
    if row['clipped'] or not 10.<polygon.area<70. or not 3.5<=height<=12. or parameters['minHeight']!=0.:
        return RoofChoice('flat',0.,top,0.,'procedural default','current small-body roof scope; larger complete room controls retain flat roofs')
    if sum(len(r.coords)-1 for r in (polygon.exterior,*polygon.interiors))>32:
        return RoofChoice('flat',0.,top,0.,'procedural default','small-body boundary exceeds the 32-corner experiment scope')
    points=np.asarray(polygon.minimum_rotated_rectangle.exterior.coords)[:4]
    lengths=np.linalg.norm(np.roll(points,-1,axis=0)-points,axis=1);width=float(lengths.min());long=float(lengths.max())
    if width<2.5:return RoofChoice('flat',0.,top,0.,'procedural default','insufficient contained width for a small-house roof estimate')
    transform,_=frame(polygon);axis=np.array(transform[:2]);angle=math.degrees(math.atan2(axis[1],axis[0]))
    kind='ridge' if long/width>=1.4 else 'shed'
    rise=min(1.6,width*.22,height-2.64)
    ring=np.asarray(shapely.orient_polygons(polygon).exterior.coords);edges=np.diff(ring,axis=0)
    edge_index=int(np.argmax(abs((edges/np.linalg.norm(edges,axis=1,keepdims=True))@axis)))
    return RoofChoice(kind,rise,top-rise,angle,'procedural size/height estimate',
        'small ordinary source envelope; shape/use unproven, no specific architecture-family match',edge_index)


def roof_surface(row,choice):
    polygon=row['polygon'];x0,y0,x1,y1=polygon.bounds;origin=np.array([(x0+x1)/2,(y0+y1)/2])
    source=Shape.from_polygon(translate(polygon,*-origin),0.,material=9).rotate_scope(0.,choice.ridge_degrees,0.)
    shape=source.roof_ridge(choice.rise_m,'byHeight') if choice.kind=='ridge' else source.roof_shed(choice.rise_m,index=choice.edge_index,mode='byHeight')
    mesh=shape.emit();mesh['vertices']=cga_to_enu(mesh['vertices']);mesh['vertices'][:,:2]+=origin
    if 'normals' in mesh:mesh['normals']=cga_to_enu(mesh['normals'])
    mesh['vertices'][:,2]+=choice.eave_m
    known_top=number(row['properties'].get('height',row['properties'].get('render_height',choice.eave_m+choice.rise_m)))
    tolerance=16*np.finfo(float).eps*max(1.,abs(known_top))
    for height in (choice.eave_m,known_top):
        values=mesh['vertices'][:,2];values[abs(values-height)<=tolerance]=height
    selected=np.asarray(['roof.top' in face.tags for face in shape.geometry.polygons])[mesh['parts']]
    top={key:value[selected] if key in ('faces','materials','groups','parts','face_tag_mask') else value for key,value in mesh.items()}
    return top,mesh,shape


def projected_error(row,parameters,choice,view):
    x0,y0,x1,y1=row['polygon'].bounds
    points=np.array([(x,y,z) for x in (x0,x1) for y in (y0,y1) for z in (parameters['minHeight'],parameters['buildingHeight'])])
    depth=float(np.min((points-view.eye)@view.basis[2]))
    factor=math.sqrt(1+(view.width/(2*view.focal_px))**2+(view.height/(2*view.focal_px))**2)
    return choice.rise_m*(view.focal_px*factor+.5)/depth if depth>0 else float('inf')


class OrdinaryDetails:
    def __init__(self,rows,view):
        self.rooms=RoomDetails(rows,view);self.view=view;self.choices=Counter();self.completed=[];self.rejected=[];self.coarse=[]

    def __call__(self,row,parameters,material):
        choice=choose(row,parameters);self.choices[choice.kind]+=1
        if choice.kind=='flat':return self.rooms(row,parameters,material)
        try:return self.complete(row,parameters,material,choice)
        except (ValueError,NotImplementedError,shapely.errors.GEOSException) as error:
            self.rejected.append(dict(id=row['id'],kind=choice.kind,reason=str(error)));return None

    def complete(self,row,parameters,material,choice):
        error=projected_error(row,parameters,choice,self.view)
        metadata=dict(roof_kind=choice.kind,roof_rise_m=choice.rise_m,eave_m=choice.eave_m,ridge_degrees=choice.ridge_degrees,
            selected_eave_edge=choice.edge_index,
            roof_origin=choice.origin,roof_reason=choice.reason,projected_geometry_error_px=error if math.isfinite(error) else None,
            functional_status='Exterior heightfield envelope; room/use/access programme and native integration unplanned')
        if error<=.5:
            mesh,info=prism(row,parameters,material);self.coarse.append(dict(id=row['id'],**metadata))
            return mesh,info|metadata|dict(roof_product='direct subpixel heightfield approximation; logical roof choice retained')
        start=time.perf_counter();top,_,_=roof_surface(row,choice)
        envelope=heightfield_envelope(top,parameters['minHeight'],wall_material=material,base_material=material)
        mesh=dict(envelope.mesh)
        for key in ('parts','groups','face_tag_mask'):mesh.pop(key)
        region=shapely.from_wkb(envelope.footprint_wkb)
        if region.symmetric_difference(row['polygon']).area>1e-7:raise ValueError('Roof envelope changed its source footprint or filled a courtyard')
        self.completed.append(dict(id=row['id'],triangles=len(mesh['faces']),build_ms=(time.perf_counter()-start)*1000,**metadata))
        return mesh,metadata|dict(form='Closed roof heightfield envelope',triangles=len(mesh['faces']))

    def report(self):
        return dict(choices=dict(self.choices),completed=self.completed,coarse=self.coarse,rejected=self.rejected,
            scope='Bounded small ordinary envelope prototypes; existing larger complete room controls remain flat. No architecture-family/native LOD admission')
