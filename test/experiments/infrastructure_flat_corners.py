"""Round concave curb corners while retaining the existing occupied road surface."""

import time

import numpy as np
import shapely


def round_ring(points,radius,error,outer):
    points = points[:-1]
    ccw = shapely.is_ccw(shapely.LinearRing(points))
    if ccw!=outer:
        points = points[::-1]
    incoming = points-np.roll(points,1,axis=0)
    outgoing = np.roll(points,-1,axis=0)-points
    before,after = np.linalg.norm(incoming,axis=1),np.linalg.norm(outgoing,axis=1)
    u,v = incoming/before[:,None],outgoing/after[:,None]
    cross = u[:,0]*v[:,1]-u[:,1]*v[:,0]
    turn = np.arctan2(cross,np.einsum('ij,ij->i',u,v))
    selected = (turn< -1e-7)&(turn> -np.pi+1e-7)
    positions = np.flatnonzero(selected)
    tangent = np.tan(-turn[positions]*.5)
    cut = np.minimum(radius*tangent,.45*np.minimum(before[positions],after[positions]))
    actual = cut/tangent
    displacement = actual*(1/np.cos(turn[positions]*.5)-1)
    significant = displacement>error
    positions,cut,actual = positions[significant],cut[significant],actual[significant]
    selected[:] = False
    selected[positions] = True
    count = np.zeros(len(points),dtype=int)
    count[positions] = np.ceil(-turn[positions]/(2*np.arccos(1-np.minimum(error/actual,1)))).astype(int)
    cuts,radii = np.zeros(len(points)),np.zeros(len(points))
    cuts[positions],radii[positions] = cut,actual
    result = []
    for i,p in enumerate(points):
        if not selected[i]:
            result.append(p[None])
            continue
        a,b = p-u[i]*cuts[i],p+v[i]*cuts[i]
        centre = a+np.array([u[i,1],-u[i,0]])*radii[i]
        radial = a-centre
        angle = np.linspace(0,turn[i],count[i]+1)
        c,s = np.cos(angle),np.sin(angle)
        arc = centre+np.c_[radial[0]*c-radial[1]*s,radial[0]*s+radial[1]*c]
        arc[0],arc[-1] = a,b
        result.append(arc)
    return np.concatenate(result),int(selected.sum()),int(count.max(initial=0))


def rounded_plan(complete,radius=2.,error=.025):
    began = time.perf_counter()
    parts = shapely.get_parts(complete)
    polygons,rounded,rejected,maximum = [],0,0,0
    for polygon in parts:
        shell,count,segments = round_ring(shapely.get_coordinates(polygon.exterior),radius,error,True)
        holes = []
        for ring in polygon.interiors:
            hole,more,n = round_ring(shapely.get_coordinates(ring),radius,error,False)
            holes.append(hole)
            count,segments = count+more,max(segments,n)
        candidate = shapely.Polygon(shell,holes)
        if not candidate.is_valid or polygon.difference(candidate).area>max(1e-7,polygon.area*1e-12):
            rejected += 1
            polygons.append(polygon)
        else:
            rounded += count
            maximum = max(maximum,segments)
            polygons.append(candidate)
    result = shapely.union_all(polygons)
    missing = complete.difference(result).area
    if missing>max(1e-7,complete.area*1e-12):
        raise ValueError('rounded curbs remove an existing transport surface')
    return result,dict(rounded_corners=rounded,retained_original_components=rejected,
                       maximum_arc_segments=maximum,corner_radius_m=radius,arc_error_m=error,
                       added_area_m2=result.area-complete.area,missing_original_area_m2=missing,
                       curve_plan_ms=(time.perf_counter()-began)*1000,
                       scope='Curb boundaries only; smooth vehicle trajectories remain open.')
