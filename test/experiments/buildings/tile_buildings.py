"""Entire pinned provider tiles with accounted buildings and procedural radial prototypes."""

from collections import Counter
import hashlib
import json
import math
from pathlib import Path

import mapbox_vector_tile
import numpy as np
import shapely

from building_mesh import solid
from provider_contract import attributes,srgb
from samples import GIRTH_M,SOURCES
from special_sources import INVENTORY,tiles
from special_forms import profile
from outshine_ref.buildings.structures import classify_structure
from outshine_ref.buildings.modelling.radial_mesh import radial_mesh


SITES=('Spittelau','BerlinFernsehturm','Grohnde','Hameln')


def read_tile(site):
    inventory=json.loads(INVENTORY.read_text());selection=next(s for s in inventory['sites'] if s['name']==site)
    tile=tiles(selection['latitude'],selection['longitude'],0)[0]
    receipt=next(r for r in inventory['receipts'] if tuple(r['tile'])==tile)
    path=SOURCES/(hashlib.sha256(receipt['url'].encode()).hexdigest()+'.pbf');data=path.read_bytes()
    if hashlib.sha256(data).hexdigest()!=receipt['sha256']:raise ValueError('Pinned tile changed')
    layers=mapbox_vector_tile.decode(data,default_options={'y_coord_down':True})
    span=GIRTH_M/(1<<tile[0])*math.cos(math.radians(selection['latitude']))
    rows=[];invalid=[]
    for name in ('building','landuse'):
        layer=layers.get(name,{});extent=layer.get('extent',4096)
        for ordinal,feature in enumerate(layer.get('features',[])):
            geometry=shapely.from_geojson(json.dumps(feature['geometry']))
            for part,piece in enumerate(shapely.get_parts(geometry)):
                xy=shapely.get_coordinates(piece)
                shape=shapely.transform(piece,lambda xy: (xy/extent-.5)*[span,-span])
                identifier=f'{site}/{tile[0]}/{tile[1]}/{tile[2]}/{name}/{feature.get("id","unknown")}/{ordinal}/{part}'
                if shape.geom_type!='Polygon' or not shape.is_valid or shape.is_empty:
                    invalid.append(dict(id=identifier,layer=name,reason='invalid or unsupported polygon'));continue
                rows.append(dict(id=identifier,layer=name,polygon=shape,properties=feature['properties'].copy(),
                    coordinate_step_m=span/extent,clipped=bool(np.any(xy<=0) or np.any(xy>=extent))))
    return rows,dict(site=site,receipt=receipt,span_m=span,invalid=invalid,
        coordinates='ENU local metres, centred on tile; Mercator scaled at selected latitude',
        scope='Every delivered building polygon in the complete tile, including provider buffer; no radius selection')


def prism(row,parameters,material):
    storage=([],[],[],[]);solid(storage,row['polygon'],parameters['minHeight'],parameters['buildingHeight'],material)
    vertices=np.asarray(storage[0]);faces=np.asarray(storage[1],dtype=np.uint32)
    triangles=vertices[faces];normals=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0])
    lengths=np.linalg.norm(normals,axis=1)
    if np.any(lengths==0) or not np.isfinite(lengths).all():raise ValueError('Unresolved prism triangle')
    normals/=lengths[:,None];corner_normals=np.zeros_like(vertices)
    np.add.at(corner_normals,faces.reshape(-1),np.repeat(normals,3,axis=0))
    corner_normals/=np.linalg.norm(corner_normals,axis=1)[:,None]
    materials=np.full(len(faces),material,dtype=np.uint16);materials[normals[:,2]>.5]=9
    return dict(vertices=vertices,faces=faces,normals=corner_normals,uv=np.zeros((len(vertices),2)),materials=materials),dict(form='supplied closed footprint prism; facade/interior programme not applied')


def construct(site,error_m=1.,prepared=None):
    rows,receipt=read_tile(site) if prepared is None else prepared
    industry=shapely.union_all([r['polygon'] for r in rows if r['layer']=='landuse' and r['properties'].get('class')=='industrial'])
    invalid_buildings=[r for r in receipt['invalid'] if r['layer']=='building']
    meshes=[];members=[];omissions=list(invalid_buildings);colours={};palette={};counts=Counter();offset=0
    for row in rows:
        if row['layer']!='building':continue
        try:
            raw_colour={k:v for k,v in row['properties'].items() if k in ('colour','color','building:colour')}
            colour_note='supplied colour' if raw_colour else 'procedural neutral'
            try:colour=tuple(srgb(row['properties'],default=(.72,.71,.68)))
            except ValueError:
                colour=(.72,.71,.68);colour_note='unresolved supplied colour retained; procedural neutral'
            params=attributes({k:v for k,v in row['properties'].items() if k not in raw_colour})
            if params['hide3d']:
                omissions.append(dict(id=row['id'],reason='provider_hide_3d'));continue
            coverage=min(1.,row['polygon'].intersection(industry).area/row['polygon'].area)
            decision=classify_structure(row['polygon'],row['properties'],coordinate_step_m=row['coordinate_step_m'],
                industrial_coverage=coverage,clipped=row['clipped'])
            metal=decision.kind in ('mast','antenna')
            key=colour+(metal,)
            if key not in colours:
                material=12+len(colours);colours[key]=material;palette[str(material)]=dict(shader='CityEnginePBRShader',
                    **{'color.'+axis:v for axis,v in zip(('r','g','b'),colour,strict=True)},roughness=.46 if metal else .78,metallic=.8 if metal else 0.)
            material=colours[key];prototype=profile(row['id'],row['polygon'],decision,(material,material,9))
            if prototype:
                plan,info=prototype;metric_error=error_m(row,params) if callable(error_m) else error_m
                result=radial_mesh(plan,error_m=metric_error)
                mesh=dict(result.mesh);info.update(profile_knots=plan.knots,profile_indices=result.profile_indices,sides=result.sides,
                    radial_error_m=result.error_m,logical_profile='complete camera-independent profile',triangles=len(mesh['faces']))
            else:
                mesh,info=prism(row,params,material)
            vertices=mesh['vertices'];expected=(params['minHeight'],params['buildingHeight'])
            if not np.array_equal([vertices[:,2].min(),vertices[:,2].max()],expected):raise ValueError('Generated body changed provider height interval')
            meshes.append(dict(vertices=vertices,faces=mesh['faces']+offset,normals=mesh['normals'],uv=mesh['uv'],materials=mesh['materials']))
            offset+=len(vertices);counts[decision.kind]+=1
            members.append(dict(id=row['id'],source_properties=row['properties'],footprint_wkb=row['polygon'].wkb_hex,
                kind=decision.kind,origin=decision.origin,alternatives=decision.alternatives,prototype=info,
                clipped=row['clipped'],source_height_interval=expected,colour_origin=colour_note))
        except (ValueError,shapely.errors.GEOSException) as error:
            omissions.append(dict(id=row['id'],reason=str(error)))
    if not meshes:raise ValueError('Tile contains no valid generated building')
    mesh={key:np.concatenate([m[key] for m in meshes]) for key in meshes[0]}
    delivered=sum(r['layer']=='building' for r in rows)+len(invalid_buildings)
    if len(members)+len(omissions)!=delivered:raise ValueError('Whole-tile source accounting incomplete')
    return mesh,receipt|dict(delivered_building_polygons=delivered,generated=len(members),counts=dict(counts),
        members=members,omissions=omissions,material_definitions=palette,terrain='flat diagnostic plane; no DEM/transport/vegetation integration')
