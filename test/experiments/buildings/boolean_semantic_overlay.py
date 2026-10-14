"""Partition Boolean surfaces by both sources, sharing original-edge nodes in 3D."""

from dataclasses import dataclass,replace

import numpy as np
import shapely

from outshine_ref.buildings.modelling.uv_exterior import polygon_parts
from boolean_overlay_fans import conform
from boolean_overlay_nodes import OverlayNodes
from boolean_overlay_regions import regions
from boolean_plane_regions import partitions


@dataclass(frozen=True)
class SemanticOverlay:
    product: object
    supports: tuple
    native_triangles: object
    added_roots: int
    precision_m: float


def interpolation(source,number,points):
    triangle=source.mesh['faces'][number];corners=source.mesh['vertices'][triangle]
    first,last=corners[1:]-corners[0];delta=points-corners[0]
    inverse=np.linalg.inv(np.array([[first@first,first@last],[first@last,last@last]]))
    pair=np.c_[delta@first,delta@last]@inverse.T;weights=np.c_[1.-pair.sum(axis=1),pair]
    if weights.min()<-1e-7:raise ValueError('Semantic attribute point left its preferred source triangle')
    return np.concatenate([weights@source.mesh['uv'+str(c) if c else 'uv'][triangle] for c in range(10)],axis=1)


def triangulate(number,parts,nodes,max_triangles):
    _,normal,_,axes=nodes.projection(number)
    polygons=[];root_ids=[];templates=[]
    for region in parts:
        rings=[]
        for ring in (region.polygon.exterior,*region.polygon.interiors):
            roots=[nodes.point(number,xy) for xy in np.asarray(ring.coords)[:-1]]
            roots=[r for i,r in enumerate(roots) if r!=roots[i-1]]
            if len(roots)<3:continue
            root_ids.extend(roots);rings.append(np.asarray([nodes.coordinates[r][axes] for r in roots]))
        if not rings:continue
        polygon=shapely.Polygon(rings[0],rings[1:])
        if not polygon.is_valid:
            repaired=shapely.make_valid(polygon)
            if abs(repaired.area-region.polygon.area)>8*nodes.precision*region.polygon.length:
                raise ValueError('Derived semantic region repair exceeds coordinate uncertainty')
            polygon=repaired
        for part in polygon_parts(polygon,max_triangles):
            polygons.append(part);templates.append(region.supports)
    if not polygons:return np.empty((0,3),dtype=np.int64),()
    source=np.asarray(polygons,dtype=object)
    triangles,owners=shapely.get_parts(shapely.constrained_delaunay_triangles(source),return_index=True)
    if len(triangles)>max_triangles:raise ValueError('Semantic triangulation capacity exceeded')
    corners=shapely.get_coordinates(triangles).reshape(-1,4,2)[:,:3]
    roots=np.asarray([nodes.point(number,xy) for xy in corners.reshape(-1,2)],dtype=np.int64).reshape(-1,3)
    roots,supports=conform(roots,owners,nodes,number,templates,max_triangles)
    xyz=np.asarray([nodes.coordinates[int(r)] for r in roots.ravel()]).reshape(-1,3,3)
    normals=np.cross(xyz[:,1]-xyz[:,0],xyz[:,2]-xyz[:,0]);reverse=normals@normal<0
    roots[reverse]=roots[reverse][:,[0,2,1]]
    return roots,supports


def overlay(product,max_candidates=1048576,max_triangles=262144,method='triangle'):
    for size in [max_candidates,max_triangles]:
        if type(size)is not int or size<=0:raise ValueError('Semantic overlay capacities must be positive integers')
    if not len(product.mesh['faces']):return SemanticOverlay(product,(),np.empty(0,dtype=np.int32),0,0.)
    if method not in ('plane','triangle'):raise ValueError('Unknown semantic overlay method')
    sources=[source.mesh['vertices'][source.mesh['faces']] for source in product.sources]
    candidate_count=sum(np.count_nonzero(product.operands==owner)*len(sources[1-owner]) for owner in (0,1))
    if candidate_count>max_candidates:raise ValueError('Semantic coplanar candidate capacity exceeded')
    nodes=OverlayNodes(product);initial=nodes.next
    plans=partitions(product,nodes,max_triangles) if method=='plane' else tuple(
        regions(product,nodes,number,sources,max_triangles) for number in range(len(product.mesh['faces'])))
    for number in range(len(product.mesh['faces'])):
        plan=plans[number]
        for region in plan:
            for ring in (region.polygon.exterior,*region.polygon.interiors):
                for xy in np.asarray(ring.coords)[:-1]:nodes.point(number,xy)
    roots=[];supports=[];native=[];count=0
    for number,plan in enumerate(plans):
        values,owners=triangulate(number,plan,nodes,max_triangles-count)
        roots.append(values);supports.extend(owners);native.extend([number]*len(values));count+=len(values)
        if count>max_triangles:raise ValueError('Semantic output triangle capacity exceeded')
    roots=np.concatenate(roots);points=np.asarray([nodes.coordinates[int(r)] for r in roots.ravel()])
    owner=np.asarray([s[0][0] for s in supports],dtype=np.int32);triangles=np.asarray([s[0][1] for s in supports],dtype=np.int32)
    properties=np.empty((len(points),20))
    for key in set(zip(owner,triangles)):
        selected=np.flatnonzero((owner==key[0])&(triangles==key[1]));corners=(selected[:,None]*3+np.arange(3)).ravel()
        properties[corners]=interpolation(product.sources[int(key[0])],int(key[1]),points[corners])
    mesh=dict(vertices=points,faces=np.arange(len(points),dtype=np.int32).reshape(-1,3))
    mesh.update({'uv'+str(c) if c else 'uv':properties[:,2*c:2*c+2] for c in range(10)})
    result=replace(product,mesh=mesh,operands=owner,source_triangles=triangles,roots=roots.ravel())
    return SemanticOverlay(result,tuple(supports),np.asarray(native,dtype=np.int32),nodes.next-initial,nodes.precision)
