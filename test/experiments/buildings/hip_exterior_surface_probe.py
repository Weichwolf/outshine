"""Own visible exterior roof envelope; original interior field stays unchanged."""

from dataclasses import dataclass

import numpy as np
import shapely

from outshine_ref.buildings.modelling.uv_exterior import polygon_parts


@dataclass(frozen=True)
class ExteriorPatch:
    polygon: object
    gradient: tuple
    intercept_m: float
    owner: int


@dataclass(frozen=True)
class ExteriorSurface:
    patches: tuple
    domain: object
    base_level_m: float
    precision_m: float
    candidate_pairs: int
    collapsed_triangles: tuple


def polygonal(geometry,precision):
    parts=list(polygon_parts(geometry,65536))
    return shapely.union_all(parts,grid_size=precision) if parts else shapely.Polygon()


def above(polygon, gradient, intercept, level, precision):
    gradient=np.asarray(gradient,dtype=float);length=np.linalg.norm(gradient)
    if length==0:return polygon if intercept>=level else shapely.Polygon()
    direction=gradient/length;normal=np.array([-direction[1],direction[0]])
    coordinates=shapely.get_coordinates(polygon);anchor=coordinates.mean(axis=0)
    projected=(coordinates-anchor)@np.array([direction,normal]).T
    low,south=projected.min(axis=0);high,north=projected.max(axis=0)
    cut=(level-intercept-gradient@anchor)/length
    if cut<=low:return polygon
    if cut>=high:return shapely.Polygon()
    pad=max(1.,high-low,north-south)
    mask=shapely.Polygon(np.array([[cut,south-pad],[high+pad,south-pad],
                                   [high+pad,north+pad],[cut,north+pad]])@np.array([direction,normal])+anchor)
    return polygonal(shapely.intersection(polygon,mask,grid_size=precision),precision)


def candidate_strips(plan,value,mode,overhang,even,precision):
    factor=plan.factor(value,mode);distance=overhang/np.hypot(1.,factor)
    rings=(plan.footprint.exterior,*plan.footprint.interiors)
    expanded=[]
    for ring in rings:
        xy=np.asarray(ring.coords)[:-1];edges=np.roll(xy,-1,axis=0)-xy
        gradients=np.c_[-edges[:,1],edges[:,0]]/np.linalg.norm(edges,axis=1)[:,None]
        prior=np.roll(gradients,1,axis=0);denominator=1.+np.einsum('ij,ij->i',prior,gradients)
        if np.any(denominator<=1e-12):raise ValueError('Exterior miter is numerically unresolved')
        expanded.append(xy-distance*(prior+gradients)/denominator[:,None])
    footprint=shapely.set_precision(plan.footprint,precision)
    candidates=[];collapsed=[]
    for owner,facet in enumerate(plan.facets):
        ring,edge=facet.boundary;xy=np.asarray(rings[ring].coords)[edge:edge+2]
        outer=expanded[ring];vertices=np.r_[xy,outer[[(edge+1)%len(outer),edge]]]
        polygon=shapely.Polygon(vertices)
        if not polygon.is_valid:polygon=polygonal(shapely.make_valid(polygon),precision)
        polygon=shapely.set_precision(polygon,precision)
        if polygon.is_empty:collapsed.append((owner,0,'below precision'));continue
        polygon=polygonal(shapely.difference(polygon,footprint,grid_size=precision),precision)
        if polygon.is_empty:continue
        candidates.append(ExteriorPatch(polygon,tuple(np.asarray(facet.gradient)*factor),facet.intercept_m*factor,owner))
    return candidates,float(distance*factor),tuple(collapsed)


def prepare(plan,value=3.,mode='byHeight',overhang=.5,even=False,max_pairs=65536):
    if not np.isfinite(overhang) or overhang<=0:raise ValueError('Exterior surface needs a positive overhang')
    if type(max_pairs)is not int or max_pairs<=0:raise ValueError('Invalid exterior pair capacity')
    precision=max(1e-10,max(1.,max(abs(x) for x in plan.footprint.bounds))*64*np.finfo(float).eps,2*plan.maximum_refinement_m)
    candidates,drop,collapsed=candidate_strips(plan,value,mode,overhang,even,precision)
    polygons=np.asarray([c.polygon for c in candidates],dtype=object)
    pairs=shapely.STRtree(polygons).query(polygons,predicate='intersects')
    if pairs.shape[1]>max_pairs:raise ValueError('Exterior overlap candidate capacity exceeded')
    neighbors={i:[] for i in range(len(candidates))}
    for a,b in pairs.T:
        if a!=b:neighbors[int(a)].append(int(b))
    visible=[]
    for number,candidate in enumerate(candidates):
        polygon=candidate.polygon
        for other in neighbors[number]:
            overlap=polygonal(shapely.intersection(polygon,candidates[other].polygon,grid_size=precision),precision)
            if overlap.is_empty or overlap.area==0:continue
            gradient=np.asarray(candidates[other].gradient)-candidate.gradient
            intercept=candidates[other].intercept_m-candidate.intercept_m
            points=shapely.get_coordinates(overlap)
            height=points@gradient+intercept
            if abs(height).max()<=1e-7:
                hidden=overlap if other<number else shapely.Polygon()
            else:hidden=above(overlap,gradient,intercept,0.,precision)
            polygon=polygonal(shapely.difference(polygon,hidden,grid_size=precision),precision)
            if polygon.is_empty:break
        for part in shapely.get_parts(polygon):
            if part.geom_type=='Polygon' and part.area>0:
                if not part.is_valid:raise ValueError('Invalid visible exterior patch')
                visible.append(ExteriorPatch(part,candidate.gradient,candidate.intercept_m,candidate.owner))
    domain=shapely.union_all([plan.footprint,*[p.polygon for p in visible]],grid_size=precision)
    if not domain.is_valid:raise ValueError('Invalid exterior roof domain')
    return ExteriorSurface(tuple(visible),domain,-float(drop),precision,pairs.shape[1],collapsed)


def prepare_collar(plan,value=3.,mode='byHeight',overhang=.5,even=False,max_pairs=65536):
    """Continuous CDT collar with a bounded miter and a fixed outer eave height."""
    if not np.isfinite(overhang) or overhang<=0:raise ValueError('Collar needs positive overhang')
    if type(max_pairs)is not int or max_pairs<=0:raise ValueError('Invalid collar capacity')
    factor=plan.factor(value,mode);distance=overhang/np.hypot(1.,factor);drop=distance*factor
    precision=max(1e-9,max(1.,max(abs(x) for x in plan.footprint.bounds))*64*np.finfo(float).eps,2*plan.maximum_refinement_m)
    original=shapely.set_precision(plan.footprint,precision)
    domain=shapely.set_precision(shapely.buffer(original,distance,join_style='mitre',mitre_limit=5.),precision)
    annulus=shapely.difference(domain,original,grid_size=precision)
    triangles=shapely.get_parts(shapely.constrained_delaunay_triangles(annulus))
    if len(triangles)*3>max_pairs:raise ValueError('Collar triangle capacity exceeded')
    rings=(plan.footprint.exterior,*plan.footprint.interiors)
    edges=[]
    for facet in plan.facets:
        ring,edge=facet.boundary;edges.append(shapely.LineString(np.asarray(rings[ring].coords)[edge:edge+2]))
    edges=np.asarray(edges,dtype=object);patches=[]
    for triangle in triangles:
        corners=np.asarray(triangle.exterior.coords)[:3]
        inner=shapely.distance(shapely.points(corners),original.boundary)<=2*precision
        outer=shapely.distance(shapely.points(corners),domain.boundary)<=2*precision
        if not np.all(inner|outer):raise ValueError('CDT collar created a non-boundary vertex')
        heights=np.where(inner,0.,-drop)
        center=corners.mean(axis=0);center_height=heights.mean()
        for edge in range(3):
            stop=(edge+1)%3;points=np.array([corners[edge],corners[stop],center])
            levels=np.array([heights[edge],heights[stop],center_height])
            matrix=np.c_[points-points[0],np.ones(3)]
            gradient=np.linalg.solve(matrix,levels)[:2]
            intercept=float(levels[0]-points[0]@gradient)
            polygon=shapely.Polygon(points)
            segment=shapely.LineString(points[:2])
            incident=shapely.intersection(edges,segment)
            overlap=shapely.length(incident)
            if overlap.max()>segment.length-2*precision:owner=int(np.argmax(overlap))
            else:owner=int(np.argmin(shapely.distance(edges,polygon.centroid)))
            patches.append(ExteriorPatch(polygon,tuple(gradient),intercept,owner))
    return ExteriorSurface(tuple(patches),domain,-float(drop),precision,len(triangles)*3,())
