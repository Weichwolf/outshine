"""Conservative rigid vehicle clearance along analytic turn poses."""

import numpy as np
import shapely

from infrastructure_clothoids import sample_clothoids
from infrastructure_clothoid_turns import sample_symmetric_turns


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
    envelope = shapely.buffer(shapely.polygons(outlines),deviation/np.cos(np.pi/32),quad_segs=8)
    shapely.prepare(surface)
    inside = shapely.covers(surface,envelope)
    return dict(fits_surface=bool(np.all(inside)),outside_poses=int(np.count_nonzero(~inside)),
                sampled_poses=len(positions),sweep_deviation_bound_m=float(deviation)),outlines


def turn_body_check(surface,fit,begin,end,profile,vehicle,error=.025,maximum_poses=4096):
    half_width = vehicle['widthM']*.5
    front,rear = vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']
    curvature,length = float(fit['maximum_curvature_per_m'][0]),float(fit['length_m'][0])
    radius = np.hypot(max(front,rear),half_width)
    spacing = 2*error/(1+curvature*radius)
    before,after = np.linalg.norm(fit['start'][0]-begin),np.linalg.norm(end-fit['end'][0])
    counts = np.ceil(np.array([before,length,after])/spacing).astype(int)
    counts[1] = max(counts[1],1)
    if counts.sum()+1>maximum_poses:
        return dict(fits_surface=False,reason='vehicle_sweep_budget_exhausted'),None
    stations = np.linspace(0,1,counts[1]+1)
    xy,headings = (sample_clothoids(fit,stations) if profile=='G1' else
                   sample_symmetric_turns(fit,stations)[:2])
    prefix = np.linspace(begin,xy[0,0],counts[0]+1)[:-1]
    suffix = np.linspace(xy[0,-1],end,counts[2]+1)[1:]
    positions = np.vstack((prefix,xy[0],suffix))
    angles = np.r_[np.full(len(prefix),headings[0,0]),headings[0],np.full(len(suffix),headings[0,-1])]
    step = np.divide([before,length,after],counts,out=np.zeros(3),where=counts>0).max()
    return sweep_check(surface,positions,angles,curvature,step,half_width,front,rear)
