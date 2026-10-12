"""Metrical window/entrance components attached to an existing facade opening."""

import numpy as np
import shapely

from building_mesh import solid


FRAME_MATERIAL = 10
SILL_MATERIAL = 11
FRAME_M = .055
CASING_M = .055


def ring(width, height, border, outside=False):
    outer = shapely.box(-width/2,0.,width/2,height)
    return outer.buffer(border,join_style='mitre').difference(outer) if outside else outer.difference(
        shapely.box(-width/2+border,border,width/2-border,height-border))


def profile(storage, polygon, front, back, method):
    first = len(storage[0])
    if method=='ring':
        solid(storage,polygon,-back,-front,FRAME_MATERIAL)
    elif method=='bars':
        x0,z0,x1,z1 = polygon.bounds
        hole = shapely.Polygon(polygon.interiors[0])
        a,b,c,d = hole.bounds
        for part in (shapely.box(x0,z0,x1,b),shapely.box(x0,d,x1,z1),
                     shapely.box(x0,b,a,d),shapely.box(c,b,x1,d)):
            solid(storage,part,-back,-front,FRAME_MATERIAL)
    else:
        raise ValueError('Unknown opening-profile construction')
    points = np.asarray(storage[0][first:])
    storage[0][first:] = (points[:,[0,2,1]]*np.array([1.,-1.,1.])).tolist()


def sill(storage, width):
    back,front = -.045,-.16
    top,bottom = -CASING_M,-CASING_M-.032
    drop = (back-front)/12.
    section = shapely.Polygon([(front,bottom-drop),(back,bottom),(back,top),(front,top-drop)])
    first = len(storage[0])
    solid(storage,section,-width/2,width/2,SILL_MATERIAL)
    points = np.asarray(storage[0][first:])
    storage[0][first:] = points[:,[2,0,1]].tolist()
    solid(storage,shapely.box(-width/2,back,width/2,0.),bottom,top,SILL_MATERIAL)


def template(kind='window', width=1.20, height=1.20, method='ring'):
    if kind not in ('window','entrance') or not np.isfinite([width,height]).all() or min(width,height)<=2*FRAME_M:
        raise ValueError('Opening module has invalid kind or dimensions')
    storage = [],[],[],[]
    if kind=='window':
        profile(storage,ring(width,height,FRAME_M),.043,.118,method)
        profile(storage,ring(width,height,CASING_M,outside=True),-.045,0.,method)
        sill(storage,width+2*CASING_M+.05)
    else:
        b = CASING_M
        for part in (shapely.box(-width/2-b,0.,-width/2,height),
                     shapely.box(width/2,0.,width/2+b,height),
                     shapely.box(-width/2-b,height,width/2+b,height+b)):
            first = len(storage[0])
            solid(storage,part,0.,.045,FRAME_MATERIAL)
            points = np.asarray(storage[0][first:])
            storage[0][first:] = (points[:,[0,2,1]]*np.array([1.,-1.,1.])).tolist()
    return dict(vertices=np.asarray(storage[0],dtype=np.float64),faces=np.asarray(storage[1],dtype=np.int32),
                materials=np.asarray(storage[2],dtype=np.uint8),parts=np.asarray(storage[3],dtype=np.int32))


def place(product, centre, tangent, bottom):
    tangent = np.asarray(tangent,dtype=np.float64)
    if not np.isfinite(tangent).all() or abs(np.linalg.norm(tangent)-1.)>1e-8:
        raise ValueError('Opening module requires a unit facade tangent')
    rotation = np.array([[tangent[0],-tangent[1],0.],[tangent[1],tangent[0],0.],[0.,0.,1.]])
    origin = np.array([*centre,bottom])
    return product|dict(vertices=product['vertices']@rotation.T+origin)
