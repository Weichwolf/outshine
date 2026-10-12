"""Closed roof slabs and attic-wall infill attached to the constructive storey envelope."""

import numpy as np
import shapely

from building_layout import WALL_M, canonical_geometry, polygons
from building_mesh import solid
from roof_planes import height


THICKNESS_M = .18
ROOF_MATERIAL = 9


def above(facet, origin, threshold):
    gradient = np.asarray(facet['plane'][:2])
    length = np.linalg.norm(gradient)
    if length < 1e-10:
        return facet['polygon'] if facet['plane'][2] > threshold else shapely.Polygon()
    normal = gradient/length
    tangent = np.array([-normal[1],normal[0]])
    points = np.array(facet['polygon'].exterior.coords)-origin
    size = np.max(np.linalg.norm(points,axis=1))*2+1.
    cut = (threshold-facet['plane'][2])/length
    shape = shapely.Polygon([origin+tangent*x+normal*y for x,y in
                             ((-size,cut),(size,cut),(size,size),(-size,size))])
    return facet['polygon'].intersection(shape)


def component(storage, polygon, facet, roof, eave, rise, infill=False, ceiling=False):
    first = len(storage[0])
    variable = infill or ceiling
    solid(storage,polygon,0. if variable else -THICKNESS_M,1. if variable else 0.,2 if ceiling else 1 if infill else ROOF_MATERIAL)
    points = np.asarray(storage[0][first:])
    top = height(facet,points[:,:2],roof['origin'])*rise
    if variable:
        thickness = top-(0. if ceiling else THICKNESS_M)
        if thickness.min() < -1e-7:
            raise ValueError('derived roof contact has negative solid thickness')
        thickness[abs(thickness)<1e-7] = 0.
        if ceiling:
            thickness[abs(thickness-THICKNESS_M)<1e-7] = THICKNESS_M
        points[:,2] = eave+points[:,2]*thickness
    else:
        points[:,2] += eave+top
    storage[0][first:] = points.tolist()


def mesh(roof, footprint, eave, rise):
    storage = [],[],[],[]
    wall = canonical_geometry(footprint.difference(footprint.buffer(-WALL_M,join_style='mitre')))
    for facet in roof['facets']:
        component(storage,facet['polygon'],facet,roof,eave,rise)
        derived = facet|dict(polygon=canonical_geometry(facet['polygon']))
        higher = canonical_geometry(above(derived,roof['origin'],THICKNESS_M/rise))
        for part in polygons(canonical_geometry(derived['polygon'].difference(higher))):
            component(storage,part,facet,roof,eave-THICKNESS_M,rise,ceiling=True)
        solid(storage,higher,eave-THICKNESS_M,eave,2)
        if rise>THICKNESS_M:
            infill = canonical_geometry(higher.intersection(wall))
            for part in polygons(infill):
                component(storage,part,facet,roof,eave,rise,infill=True)
    vertices = np.asarray(storage[0],dtype=np.float64)
    faces = np.asarray(storage[1],dtype=np.int32)
    triangles = vertices[faces]
    usable = np.linalg.norm(np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]),axis=1)>1e-10
    _,indices = np.unique(vertices,axis=0,return_inverse=True)
    keys = np.c_[storage[3],np.sort(indices[faces],axis=1)]
    _,inverse,counts = np.unique(keys,axis=0,return_inverse=True,return_counts=True)
    duplicates = counts[inverse]>1
    if np.any(duplicates&((counts[inverse]!=2)|(np.ptp(triangles[:,:,2],axis=1)!=0)|(np.asarray(storage[2])==ROOF_MATERIAL))):
        raise ValueError('unexpected coincident roof-component faces')
    usable &= ~duplicates
    return dict(vertices=vertices,faces=faces[usable],materials=np.asarray(storage[2],dtype=np.uint8)[usable],
                parts=np.asarray(storage[3],dtype=np.int32)[usable])


def attach(body, roof, footprint, eave, rise, storeys):
    if roof['kind']=='flat':
        materials = body['materials'].copy()
        materials[(materials==2)&(body['levels']==storeys-1)] = ROOF_MATERIAL
        return body|dict(materials=materials)
    top = mesh(roof,footprint,eave,rise)
    selected = ~((body['materials']==2)&(body['levels']==storeys-1))
    return dict(vertices=np.r_[body['vertices'],top['vertices']],
                faces=np.r_[body['faces'][selected],top['faces']+len(body['vertices'])],
                materials=np.r_[body['materials'][selected],top['materials']],
                parts=np.r_[body['parts'][selected],top['parts']+int(body['parts'].max())+1],
                levels=np.r_[body['levels'][selected],np.full(len(top['faces']),storeys,dtype=np.uint8)])
