"""Choose shared lane poses without losing any neighboring directed route."""

import heapq

import numpy as np
import shapely

from infrastructure_vehicle_sweep import body_outlines


def frame_key(pose):
    return pose['section'],pose['edge'],pose['direction'],pose['station']


def accepted_routes(report):
    return {(t['entry'],t['exit']) for t in report['turns'] if t['accepted']}


def preserves_routes(before,after):
    requests = lambda r:{(t['entry'],t['exit']) for t in r['turns']}
    return (before.keys()==after.keys() and all(requests(before[k])==requests(after[k]) and
            accepted_routes(before[k])<=accepted_routes(after[k]) for k in before))


def heading_choices(pose,anchor,surfaces,vehicle,error,step_degrees=5.):
    if step_degrees<=0 or not np.isfinite(step_degrees):
        raise ValueError('heading step must be finite and positive')
    angles = np.deg2rad(np.arange(-30.,30.+step_degrees*.5,step_degrees))
    angles = angles[np.abs(angles)<=np.deg2rad(30.)+1e-12]
    source = np.arctan2(anchor[1],anchor[0])
    current = np.arctan2(pose['heading'][1],pose['heading'][0])
    headings = np.r_[current,source+angles]
    outlines = body_outlines(np.repeat(pose['position'][None,:],len(headings),axis=0),headings,
                             vehicle['widthM']*.5,vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
    bodies = shapely.polygons(outlines)
    valid = np.ones(len(headings),dtype=bool)
    for surface in surfaces:
        shapely.prepare(surface)
        valid &= shapely.covers(surface,bodies)&(shapely.distance(bodies,shapely.boundary(surface))>=error)
    result = []
    for angle in headings[valid]:
        heading = np.array([np.cos(angle),np.sin(angle)])
        if any(np.linalg.norm(heading-h)<1e-10 for h,_ in result):
            continue
        deviation = abs(np.arctan2(np.sin(angle-current),np.cos(angle-current)))
        result.append((heading,float(deviation)))
    return result


def section_pose_domain(pose,lane,surfaces,vehicle,error):
    ends = shapely.get_coordinates(lane['section'])
    tangent = (ends[-1]-ends[0])/lane['section'].length
    positions = pose['position']+np.linspace(-1,1,17)[:,None]*lane['half_width']*tangent
    source = np.arctan2(lane['heading'][1],lane['heading'][0])
    current = np.arctan2(pose['heading'][1],pose['heading'][0])
    angles = np.r_[current,source+np.deg2rad(np.arange(-30,31,5))]
    headings = np.stack((np.cos(angles),np.sin(angles)),axis=1)
    duplicates = np.linalg.norm(headings[:,None]-headings[None,:],axis=2)<1e-10
    angles,headings = angles[~np.any(np.tril(duplicates,-1),axis=1)],headings[~np.any(np.tril(duplicates,-1),axis=1)]
    points,rotations = np.repeat(positions,len(angles),axis=0),np.tile(angles,len(positions))
    directions = np.tile(headings,(len(positions),1))
    bodies = shapely.polygons(body_outlines(points,rotations,vehicle['widthM']*.5,
                                           vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']))
    valid = shapely.distance(shapely.points(points),lane['section'])<=1e-7
    for surface in surfaces:
        shapely.prepare(surface)
        valid &= shapely.covers(surface,bodies)&(shapely.distance(bodies,shapely.boundary(surface))>=error)
    indices = np.flatnonzero(valid)
    coordinates,owners = shapely.get_coordinates(shapely.intersection(bodies[indices],lane['section']),return_index=True)
    right = np.array([lane['heading'][1],-lane['heading'][0]])
    lateral = np.einsum('ij,j->i',coordinates-lane['origin'],right)
    if lane['separated']:
        side = np.sign(np.sum((pose['position']-lane['origin'])*right))
        if side==0:
            return []
        lateral *= side
    low,high = np.full(len(bodies),np.inf),np.full(len(bodies),-np.inf)
    np.minimum.at(low,indices[owners],lateral)
    np.maximum.at(high,indices[owners],lateral)
    valid &= np.isfinite(low)&np.isfinite(high)
    valid &= (low>=(error if lane['separated'] else -lane['half_width']+error))&(high<=lane['half_width']-error)
    return [dict(position=points[k],heading=directions[k]) for k in np.flatnonzero(valid)]


def pose_cost(candidate,current,vehicle):
    a,b = current['heading'],candidate['heading']
    angle = abs(np.arctan2(a[0]*b[1]-a[1]*b[0],np.sum(a*b)))
    return float(np.linalg.norm(candidate['position']-current['position'])+vehicle['minimumRearAxleRadiusM']*angle)


def ranked_pairs(choices):
    if any(not values for values in choices):
        return
    ordered = [sorted(enumerate(values),key=lambda row:row[1][1]) for values in choices]
    heap,seen = [],set()
    def push(i,j):
        if i>=len(ordered[0]) or j>=len(ordered[1]) or (i,j) in seen:
            return
        a,b = ordered[0][i],ordered[1][j]
        heapq.heappush(heap,(a[1][1]+b[1][1],a[0],b[0],i,j))
        seen.add((i,j))
    push(0,0)
    while heap:
        _,_,_,i,j = heapq.heappop(heap)
        yield ordered[0][i][1],ordered[1][j][1]
        push(i+1,j)
        push(i,j+1)


def choose_shared_frames(entry,exit,choices,before,curve_fits,evaluate,maximum_candidates=64,required_gain=None):
    if maximum_candidates<1:
        raise ValueError('candidate budget must be positive')
    stats = dict(available=len(choices[0])*len(choices[1]),tried=0,curve_fits=0,neighbor_trials=0,
                 rejected_loss=0,rejected_target=0,maximum_candidates=maximum_candidates,accepted=False)
    for pair in ranked_pairs(choices):
        if all(all(np.linalg.norm(p[k]-c[0][k])<1e-10 for k in ('position','heading')) for p,c in zip((entry,exit),pair)):
            continue
        if stats['tried']>=maximum_candidates:
            break
        stats['tried'] += 1
        proposed = [dict(p,**c[0]) for p,c in zip((entry,exit),pair)]
        if not curve_fits(*proposed):
            continue
        stats['curve_fits'] += 1
        overrides = {frame_key(p):dict(position=p['position'],heading=p['heading']) for p in proposed}
        after = evaluate(overrides)
        stats['neighbor_trials'] += 1
        if not preserves_routes(before,after):
            stats['rejected_loss'] += 1
            continue
        if required_gain is not None and required_gain[1:] not in accepted_routes(after[required_gain[0]]):
            stats['rejected_target'] += 1
            continue
        gained = sum(len(accepted_routes(after[k])-accepted_routes(before[k])) for k in before)
        if gained:
            rotations = [np.arctan2(p['heading'][0]*c[0]['heading'][1]-p['heading'][1]*c[0]['heading'][0],
                                   np.sum(p['heading']*c[0]['heading'])) for p,c in zip((entry,exit),pair)]
            shifts = [float(np.linalg.norm(p['position']-c[0]['position'])) for p,c in zip((entry,exit),pair)]
            stats.update(accepted=True,gained_routes=gained,rotation_degrees=np.rad2deg(rotations).tolist(),
                         displacements_m=shifts)
            return overrides,after,stats
    stats['reason'] = 'frame_candidate_budget_exhausted' if stats['tried']>=maximum_candidates else 'no_valid_shared_heading'
    return {},before,stats
