"""Compare bounded turn recipes against the actual flat road surface."""

import numpy as np
import shapely

from transport_clothoids import fit_clothoids,sample_clothoids
from transport_clothoid_turns import fit_symmetric_turns,sample_symmetric_turns
from transport_vehicle_sweep import turn_body_check


def clearance_turns(node,begin,end,surface,profile='G2',minimum_radius=4.,half_width=.9,error=.025,
                    vehicle=None,transition_fraction=.5,best_only=False):
    if vehicle is not None:
        minimum_radius,half_width = vehicle['minimumRearAxleRadiusM'],vehicle['widthM']*.5
    if minimum_radius<=0 or half_width<=0 or error<=0 or profile not in ('G1','G2'):
        raise ValueError('invalid turn recipe')
    incoming,outgoing = node-begin,end-node
    available = min(np.linalg.norm(incoming),np.linalg.norm(outgoing))
    if available<=error:
        return [],[]
    incoming,outgoing = incoming/np.linalg.norm(incoming),outgoing/np.linalg.norm(outgoing)
    factors = np.array([.25,.375,.5,.625,.75,.875,1.])
    trim = available*factors
    start,finish = node-trim[:,None]*incoming,node+trim[:,None]*outgoing
    initial = np.full(len(factors),np.arctan2(incoming[1],incoming[0]))
    final = np.full(len(factors),np.arctan2(outgoing[1],outgoing[0]))
    fit = (fit_clothoids(start,finish,initial,final) if profile=='G1' else
           fit_symmetric_turns(node,trim,initial,final,transition_fraction=transition_fraction))
    curvature = fit['maximum_curvature_per_m']
    eligible = fit['valid']&(curvature*minimum_radius<=1+1e-12)
    steps = np.ceil(fit['length_m']*np.sqrt(curvature/(8*error))).astype(int)
    steps = np.maximum(steps,1)
    eligible &= steps<=255
    candidates = []
    shapely.prepare(surface)
    for index in np.flatnonzero(eligible):
        local = {k:(v[index:index+1] if isinstance(v,np.ndarray) else v) for k,v in fit.items()}
        stations = np.linspace(0,1,steps[index]+1)
        xy = (sample_clothoids(local,stations)[0] if profile=='G1' else
              sample_symmetric_turns(local,stations)[0])
        coordinates = np.vstack((begin,xy[0],end))
        coordinates = coordinates[np.r_[True,np.linalg.norm(np.diff(coordinates,axis=0),axis=1)>1e-10]]
        path = shapely.LineString(coordinates)
        candidates.append((index,path,local))
    if best_only:
        candidates.sort(key=lambda candidate:candidate[1].length)
    paths,reports = [],[]
    for index,path,local in candidates:
        tube = shapely.buffer(path,(half_width+error)/np.cos(np.pi/32),quad_segs=8)
        inside = bool(shapely.covers(surface,tube))
        body = None
        if vehicle is not None and inside:
            body,_ = turn_body_check(surface,local,begin,end,profile,vehicle,error)
            inside &= body['fits_surface']
        paths.append(path)
        reports.append(dict(profile=profile,factor=float(factors[index]),fits_surface=inside,
                            transition_fraction=transition_fraction if profile=='G2' else None,
                            minimum_radius_m=float(1/curvature[index]) if curvature[index]>0 else None,
                            length_m=path.length,arc_segments=int(steps[index]),vehicle_sweep=body,
                            closure_error_m=float(fit['closure_error_m'][index]),
                            endpoint_curvature_per_m=(0. if profile=='G2' else float(max(
                                abs(fit['begin_curvature_per_m'][index]),abs(fit['end_curvature_per_m'][index]))))))
        if best_only and inside:
            break
    return paths,reports
