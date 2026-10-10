"""Choose shared lane headings without losing any neighboring directed route."""

from itertools import product

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


def choose_shared_frames(entry,exit,choices,before,curve_fits,evaluate,maximum_candidates=64,required_gain=None):
    if maximum_candidates<1:
        raise ValueError('candidate budget must be positive')
    candidates = sorted(product(*choices),key=lambda pair:sum(c[1] for c in pair))
    stats = dict(available=len(candidates),tried=0,curve_fits=0,neighbor_trials=0,
                 rejected_loss=0,rejected_target=0,maximum_candidates=maximum_candidates,accepted=False)
    for pair in candidates:
        if all(np.linalg.norm(p['heading']-c[0])<1e-10 for p,c in zip((entry,exit),pair)):
            continue
        if stats['tried']>=maximum_candidates:
            break
        stats['tried'] += 1
        proposed = [dict(p,heading=c[0]) for p,c in zip((entry,exit),pair)]
        if not curve_fits(*proposed):
            continue
        stats['curve_fits'] += 1
        overrides = {frame_key(p):p['heading'] for p in proposed}
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
            rotations = [np.arctan2(p['heading'][0]*c[0][1]-p['heading'][1]*c[0][0],
                                   np.sum(p['heading']*c[0])) for p,c in zip((entry,exit),pair)]
            stats.update(accepted=True,gained_routes=gained,rotation_degrees=np.rad2deg(rotations).tolist())
            return overrides,after,stats
    stats['reason'] = 'frame_candidate_budget_exhausted' if stats['tried']>=maximum_candidates else 'no_valid_shared_heading'
    return {},before,stats
