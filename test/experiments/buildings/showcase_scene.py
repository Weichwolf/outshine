"""Real provider neighbours around current cached constructive buildings; no city LOD admission."""

import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely

from building_mesh import solid
from demand import View
from provider_contract import attributes
from samples import read_site
from verify_openings import decoded


LABELS = ('rectangle','vienna-compact','vienna-courtyard','tokyo-compact')


def cached(label,source,plans):
    stem='inspection-'+label+'-window'
    report=json.loads((source/(stem+'-report.json')).read_text())
    mesh_path=source/('opening-'+label+'-mesh.npz')
    digest=hashlib.sha256(mesh_path.read_bytes()).hexdigest()
    if digest!=report['renderer']['constructive_import']['mesh_sha256']:
        raise ValueError('Cached opening mesh changed since its verified inspection')
    plan=json.loads((plans/f'roof-{label}-flat-plan.json').read_text(),object_hook=decoded)
    mesh=dict(np.load(mesh_path))
    return dict(label=label,plan=plan,mesh=mesh,mesh_path=mesh_path,report=report,
                slots=json.loads((source/f'opening-{label}-slots.json').read_text()),
                plan_sha256=hashlib.sha256((plans/f'roof-{label}-flat-plan.json').read_bytes()).hexdigest())


def combine(meshes):
    rows=[];vertex_offset=part_offset=0
    for mesh in meshes:
        row=dict(vertices=mesh['vertices'],faces=mesh['faces']+vertex_offset,
                 materials=mesh['materials'],parts=mesh['parts']+part_offset,
                 levels=mesh.get('levels',np.zeros(len(mesh['faces']),dtype=np.uint8)))
        rows.append(row)
        vertex_offset+=len(mesh['vertices'])
        part_offset+=int(mesh['parts'].max())+1
    if not rows:
        raise ValueError('No valid buildings in the requested provider view')
    result={key:np.concatenate([row[key] for row in rows]) for key in rows[0]}
    if not np.isfinite(result['vertices']).all():
        raise ValueError('Provider scene contains non-finite coordinates')
    return result


def context_scene(site,focal,output,group):
    began=time.perf_counter()
    samples,receipt=read_site(site)
    centres=[body['plan']['footprint'].centroid for body in focal]
    anchor=centres[0] if centres else shapely.Point(0.,0.)
    if group:
        samples=[sample for sample in samples if sample['polygon'].distance(anchor)<=80.]
    bodies={body['plan']['id']:body for body in focal}
    palette=[];colour_ids={};storage=([],[],[],[]);meshes=[];members=[];omissions=[]
    for sample in samples:
        if sample['id'] in bodies:
            body=bodies[sample['id']];meshes.append(body['mesh'])
            members.append(dict(id=sample['id'],kind='constructive',cache_commit=body['report']['commit'],
                                mesh_sha256=hashlib.sha256(body['mesh_path'].read_bytes()).hexdigest()))
            continue
        try:
            supplied=attributes(sample['properties'])
        except ValueError as error:
            omissions.append(dict(id=sample['id'],reason=str(error)));continue
        colour=np.array([supplied[key] for key in ('wallColorR','wallColorG','wallColorB')])
        colour=np.where(colour<=.04045,colour/12.92,((colour+.055)/1.055)**2.4)
        key=tuple(colour.tolist())
        if key not in colour_ids:
            colour_ids[key]=12+len(palette);palette.append(list(key))
        first=len(storage[1]);first_vertex=len(storage[0])
        solid(storage,sample['polygon'],supplied['minHeight'],supplied['buildingHeight'],colour_ids[key])
        points=np.asarray(storage[0][first_vertex:])[np.asarray(storage[1][first:])-first_vertex]
        for index,triangle in enumerate(points,first):
            if np.all(abs(triangle[:,2]-supplied['buildingHeight'])<1e-8):
                storage[2][index]=9
        members.append(dict(id=sample['id'],kind='source-volume',properties=sample['properties'],
                            tile_boundary=sample['tile_boundary'],courtyards=len(sample['polygon'].interiors)))
    if storage[1]:
        meshes.append(dict(vertices=np.asarray(storage[0],dtype=np.float64),faces=np.asarray(storage[1],dtype=np.int32),
                           materials=np.asarray(storage[2],dtype=np.uint16),parts=np.asarray(storage[3],dtype=np.int32)))
    result=combine(meshes)
    path=output/f'{site.lower()}-{"group" if group else "city"}-mesh.npz'
    np.savez_compressed(path,**result)
    data=dict(source_products=len(samples),admitted_products=len(members),
              constructive_products=sum(row['kind']=='constructive' for row in members),
              source_volume_products=sum(row['kind']=='source-volume' for row in members),
              source_sha256=receipt['receipt']['sha256'],members=members,omissions=omissions,
              provider_omissions=receipt['omitted'],provider_rejections=receipt['rejected'],
              projection=receipt['projection'],triangles=len(result['faces']),
              raw_mesh_bytes=sum(array.nbytes for array in result.values()),
              generation_ms=dict(provider_decode_and_scene=(time.perf_counter()-began)*1000),
              extra_wall_colours=palette,tile_boundary_products=sum(row.get('tile_boundary',False) for row in members))
    (output/(path.stem+'-sources.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
    return result,path,data


def fitted(mesh,profile,opposite=False,city=False):
    points=mesh['vertices'][np.unique(mesh['faces'])]
    return fitted_points(points,profile,opposite,city)


def fitted_points(points,profile,opposite=False,city=False):
    low,high=points.min(axis=0),points.max(axis=0)
    centre=(low+high)/2
    direction=np.array([-1.,1.,1.3] if opposite else [1.,-1.,1.3 if city else .8])
    direction/=np.linalg.norm(direction)
    basis=View(centre+direction,centre,width=profile[0],height=profile[1])
    corners=np.array([[x,y,z] for x in (low[0],high[0]) for y in (low[1],high[1]) for z in (low[2],high[2])])
    projected=(corners-centre)@basis.basis.T
    required=np.maximum(abs(projected[:,0])*basis.focal_px/(profile[0]*.42),
                        abs(projected[:,1])*basis.focal_px/(profile[1]*.40))-projected[:,2]
    return View(centre+direction*(required.max()+1.),centre,width=profile[0],height=profile[1])
