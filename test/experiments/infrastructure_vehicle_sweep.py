"""Conservative rigid vehicle clearance along analytic turn poses."""

import numpy as np
import shapely

from infrastructure_clothoids import sample_clothoids
from infrastructure_clothoid_turns import sample_symmetric_turns
from infrastructure_curvature_paths import sample_curvature_paths


def body_outlines(positions,headings,half_width,front,rear):
    local = np.array([[-rear,-half_width],[front,-half_width],[front,half_width],[-rear,half_width]])
    cosine,sine = np.cos(headings)[:,None],np.sin(headings)[:,None]
    x = cosine*local[:,0]-sine*local[:,1]
    y = sine*local[:,0]+cosine*local[:,1]
    return positions[:,None,:]+np.stack((x,y),axis=-1)


def sweep_check(surface,positions,headings,maximum_curvature,maximum_station_step,
                half_width=.9,front=3.3,rear=1.1):
    """Every body point moves at most (1+curvature*radius)*station_step between poses."""
    radius = np.hypot(max(front,rear),half_width)
    deviation = (1+maximum_curvature*radius)*maximum_station_step*.5
    outlines = body_outlines(positions,headings,half_width,front,rear)
    bodies = shapely.polygons(outlines)
    shapely.prepare(surface)
    inside = shapely.covers(surface,bodies)
    covered = np.flatnonzero(inside)
    inside[covered] &= shapely.distance(bodies[covered],shapely.boundary(surface))>=deviation
    return dict(fits_surface=bool(np.all(inside)),outside_poses=int(np.count_nonzero(~inside)),
                sampled_poses=len(positions),sweep_deviation_bound_m=float(deviation)),outlines


def straight_body_check(surface,begin,end,heading,vehicle,error=.025):
    positions = np.array([begin,end])
    outlines = body_outlines(positions,np.full(2,heading),vehicle['widthM']*.5,
                            vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
    prism = shapely.convex_hull(shapely.MultiPoint(outlines.reshape(-1,2)))
    envelope = shapely.buffer(prism,error/np.cos(np.pi/32),quad_segs=8)
    shapely.prepare(surface)
    inside = shapely.covers(surface,envelope)
    sampled = shapely.covers(surface,shapely.polygons(outlines))
    return dict(fits_surface=bool(inside),outside_poses=int(np.count_nonzero(~sampled)),
                sampled_poses=2,sweep_deviation_bound_m=float(error),method='exact_prismatic_sweep'),outlines


def turn_body_check(surface,fit,begin,end,profile,vehicle,error=.025,maximum_poses=4096):
    if maximum_poses<2:
        return dict(fits_surface=False,reason='vehicle_sweep_budget_exhausted'),None
    sampler = {'G1':sample_clothoids,'G2':sample_symmetric_turns,
               'G2-polynomial':sample_curvature_paths}.get(profile)
    if sampler is None:
        raise ValueError('unknown vehicle sweep trajectory')
    xy,angles = sampler(fit,np.array([0.,1.]))[:2]
    xy,angles = xy[0],angles[0]
    curvature,length = float(fit['maximum_curvature_per_m'][0]),float(fit['length_m'][0])
    if curvature==0:
        return straight_body_check(surface,begin,end,angles[0],vehicle,error)
    radius = np.hypot(max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']),vehicle['widthM']*.5)
    rate = float(fit['maximum_curvature_rate_per_m2'][0])
    acceleration = (curvature+radius*(rate+curvature**2))*length**2
    approaches = [(a,b,angle) for a,b,angle in ((begin,xy[0],angles[0]),(xy[-1],end,angles[-1]))
                  if not np.array_equal(a,b)]
    def evaluate(owners,stations):
        positions,headings = np.empty((len(stations),2)),np.empty(len(stations))
        curved = owners==0
        if np.any(curved):
            points,heading = sampler(fit,stations[curved])[:2]
            positions[curved],headings[curved] = points[0],heading[0]
        for owner,(a,b,angle) in enumerate(approaches,1):
            mask = owners==owner
            positions[mask],headings[mask] = a+(b-a)*stations[mask,None],angle
        return positions,headings
    return swept_hull_check(surface,evaluate,np.r_[acceleration,np.zeros(len(approaches))],
                            vehicle,error,maximum_poses),None


def swept_hull_check(surface,evaluate,maximum_second_derivative,vehicle,error=.025,maximum_poses=4096):
    """A corner deviates from its endpoint chord by at most max|q''|*du**2/8."""
    acceleration = np.asarray(maximum_second_derivative,dtype=float)
    if (acceleration.ndim!=1 or not len(acceleration) or not np.isfinite(error) or error<=0 or
        np.any(~np.isfinite(acceleration)) or np.any(acceleration<0)):
        raise ValueError('invalid swept hull bounds')
    counts = np.maximum(1,np.ceil(np.sqrt(acceleration/(8*error))))
    report = dict(fits_surface=False,outside_poses=0,sampled_poses=0,sweep_deviation_bound_m=0.,
                  method='adaptive_swept_hulls',hull_intervals=0,accepted_intervals=0)
    if counts.sum()*2>maximum_poses:
        return report|dict(reason='vehicle_sweep_budget_exhausted')
    counts = counts.astype(int)
    owners = np.repeat(np.arange(len(counts)),counts)
    begin = np.concatenate([np.arange(n)/n for n in counts])
    end = begin+1/counts[owners]
    shapely.prepare(surface)
    boundary = shapely.boundary(surface)
    while len(owners):
        if report['sampled_poses']+2*len(owners)>maximum_poses:
            return report|dict(reason='vehicle_sweep_budget_exhausted')
        positions,headings = evaluate(np.repeat(owners,2),np.stack((begin,end),axis=1).ravel())
        outlines = body_outlines(positions,headings,vehicle['widthM']*.5,
                                vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
        report['sampled_poses'] += len(positions)
        report['hull_intervals'] += len(owners)
        report['outside_poses'] = int(np.count_nonzero(~shapely.covers(surface,shapely.polygons(outlines))))
        if report['outside_poses']:
            return report
        hulls = shapely.convex_hull(shapely.multipoints(outlines.reshape(-1,8,2)))
        deviation = acceleration[owners]*(end-begin)**2/8
        clear = shapely.covers(surface,hulls)&(shapely.distance(hulls,boundary)>=deviation)
        report['accepted_intervals'] += int(np.count_nonzero(clear))
        report['sweep_deviation_bound_m'] = max(report['sweep_deviation_bound_m'],
                                               float(deviation[clear].max(initial=0)))
        left,right = begin[~clear],end[~clear]
        middle = (left+right)*.5
        owners = np.repeat(owners[~clear],2)
        begin,end = np.stack((left,middle),axis=1).ravel(),np.stack((middle,right),axis=1).ravel()
    return report|dict(fits_surface=True)
