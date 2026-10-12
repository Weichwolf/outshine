import numpy as np,shapely
from transport_vehicle_sweep import body_outlines,straight_body_check
from transport_clothoids import sample_clothoids
from transport_clothoid_turns import sample_symmetric_turns
from transport_curvature_paths import sample_curvature_paths


def hull_check(surface,evaluate,acceleration,vehicle,error=.025,maximum_poses=4096):
    counts=np.maximum(1,np.ceil(np.sqrt(acceleration/(8*error))).astype(int))
    owners=np.repeat(np.arange(len(counts)),counts)
    begin=np.concatenate([np.arange(n)/n for n in counts]);end=begin+1/counts[owners]
    sampled,checked,accepted,max_deviation=0,0,0,0.
    shapely.prepare(surface);boundary=shapely.boundary(surface)
    while len(owners):
        if sampled+2*len(owners)>maximum_poses:
            return dict(fits_surface=False,reason='vehicle_sweep_budget_exhausted',sampled_poses=sampled)
        position,heading=evaluate(np.repeat(owners,2),np.stack((begin,end),axis=1).ravel())
        outlines=body_outlines(position,heading,vehicle['widthM']*.5,vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
        sampled+=len(position);checked+=len(owners)
        inside=shapely.covers(surface,shapely.polygons(outlines))
        if not np.all(inside):
            return dict(fits_surface=False,outside_poses=int((~inside).sum()),sampled_poses=sampled)
        hulls=shapely.convex_hull(shapely.multipoints(outlines.reshape(-1,8,2)))
        deviation=acceleration[owners]*(end-begin)**2/8
        clear=shapely.covers(surface,hulls)&(shapely.distance(hulls,boundary)>=deviation)
        accepted+=int(clear.sum());max_deviation=max(max_deviation,float(deviation[clear].max(initial=0)))
        failed=~clear;left,right=begin[failed],end[failed];middle=(left+right)*.5
        owners=np.repeat(owners[failed],2);begin=np.stack((left,middle),axis=1).ravel();end=np.stack((middle,right),axis=1).ravel()
    return dict(fits_surface=True,outside_poses=0,sampled_poses=sampled,sweep_deviation_bound_m=max_deviation,
                method='adaptive_swept_hulls',hull_intervals=checked,accepted_intervals=accepted)


def rate_bound(fit,profile):
    length=fit['length_m'][0]
    if profile=='G1':return abs(2*fit['bend'][0]/length**2)
    if profile=='G2':return fit['maximum_curvature_rate_per_m2'][0]
    delta,amplitude=fit['delta'][0],fit['amplitude'][0]
    a,b,c=192*amplitude,-12*delta-192*amplitude,6*delta+32*amplitude
    middle=np.clip(-b/(2*a),0,1) if a!=0 else 0.
    return max(abs(c),abs(a+b+c),abs(a*middle**2+b*middle+c))/length**2


def turn_check(surface,fit,begin,end,profile,vehicle,error=.025,maximum_poses=4096):
    curvature,length=fit['maximum_curvature_per_m'][0],fit['length_m'][0]
    sample={'G1':sample_clothoids,'G2':sample_symmetric_turns,'G2-polynomial':sample_curvature_paths}[profile]
    xy,angles=sample(fit,np.array([0.,1.]))[:2];xy,angles=xy[0],angles[0]
    if curvature==0:return straight_body_check(surface,begin,end,angles[0],vehicle,error)
    radius=np.hypot(max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']),vehicle['widthM']*.5)
    acceleration=np.array([(curvature+radius*(rate_bound(fit,profile)+curvature**2))*length**2,0.,0.])
    def evaluate(owners,u):
        positions=np.empty((len(u),2));headings=np.empty(len(u));main=owners==0
        points,heading=sample(fit,u[main])[:2];positions[main]=points[0];headings[main]=heading[0]
        for i,a,b,theta in ((1,begin,xy[0],angles[0]),(2,xy[-1],end,angles[-1])):
            mask=owners==i;positions[mask]=a+(b-a)*u[mask,None];headings[mask]=theta
        return positions,headings
    return hull_check(surface,evaluate,acceleration,vehicle,error,maximum_poses),None
