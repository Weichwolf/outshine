"""Round concave curb corners while retaining the existing occupied road surface."""

import time

import numpy as np
import shapely


def corner_patches(points,radius,error,outer,overlap):
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
    count = np.zeros(len(points),dtype=int)
    count[positions] = np.ceil(-turn[positions]/(2*np.arccos(1-np.minimum(error/actual,1)))).astype(int)
    cuts,radii = np.zeros(len(points)),np.zeros(len(points))
    cuts[positions],radii[positions] = cut,actual
    patches = []
    for i in positions:
        p = points[i]
        a,b = p-u[i]*cuts[i],p+v[i]*cuts[i]
        centre = a+np.array([u[i,1],-u[i,0]])*radii[i]
        radial = a-centre
        angle = np.linspace(0,turn[i],count[i]+1)
        c,s = np.cos(angle),np.sin(angle)
        arc = centre+np.c_[radial[0]*c-radial[1]*s,radial[0]*s+radial[1]*c]
        arc[0],arc[-1] = a,b
        inward_before = a+np.array([-u[i,1],u[i,0]])*overlap
        inward_after = b+np.array([-v[i,1],v[i,0]])*overlap
        patches.append(np.vstack((arc,inward_after,p,inward_before)))
    return patches,int(count.max(initial=0))


def rounded_plan(complete,radius=2.,error=.025,numeric_overlap=1e-7):
    began = time.perf_counter()
    parts = shapely.get_parts(complete)
    patches,maximum = [],0
    for polygon in parts:
        for ring,outer in [(polygon.exterior,True)]+[(r,False) for r in polygon.interiors]:
            local,segments = corner_patches(shapely.get_coordinates(ring),radius,error,outer,numeric_overlap)
            patches.extend(local)
            maximum = max(maximum,segments)
    omitted = []
    if patches:
        coordinates = np.concatenate(patches)
        owners = np.repeat(np.arange(len(patches)),[len(p) for p in patches])
        candidates = shapely.polygons(shapely.linearrings(coordinates,indices=owners))
        good = shapely.is_valid(candidates)&(shapely.area(candidates)>0)
        omitted = [dict(position_m=patches[i][-2].tolist(),reason='invalid_curb_patch')
                   for i in np.flatnonzero(~good)]
        result = shapely.union_all(np.r_[parts,candidates[good]])
    else:
        result = complete
    components = len(shapely.get_parts(result))
    if components>len(parts):
        raise ValueError('curb refinement created a detached surface')
    missing = complete.difference(result).area
    if missing>max(1e-7,complete.area*1e-12):
        raise ValueError('rounded curbs remove an existing transport surface')
    return result,dict(rounded_corners=len(patches)-len(omitted),omitted_curve_corners=len(omitted),omitted=omitted,
                       maximum_arc_segments=maximum,corner_radius_m=radius,arc_error_m=error,numeric_overlap_m=numeric_overlap,
                       original_components=len(parts),rounded_components=components,
                       added_area_m2=result.area-complete.area,missing_original_area_m2=missing,
                       curve_plan_ms=(time.perf_counter()-began)*1000,
                       scope='Curb boundaries only; smooth vehicle trajectories remain open.')
