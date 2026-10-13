"""Fit bounded return-turn roadbeds from tangent circle seeds."""

from itertools import chain

import numpy as np
import shapely

from transport_fitted_roadbeds import construct_fitted_roadbed
from transport_junction_surfaces import same_island_winding
from transport_three_arc_paths import three_arc_seeds
from transport_arc_line_paths import arc_line_arc_seeds
from transport_turn_roadbeds import roadbed_lateral_reach


def construct_return_roadbed(entry,exit,surface,reservations,source_route,vehicle,error=.025,maximum_calls=1024,maximum_starts=2):
    if maximum_calls<1 or maximum_starts<1:raise ValueError('invalid return-turn fitting budget')
    if np.dot(entry['heading'],exit['heading'])>=0:
        return None,dict(reason='not_a_return_turn',residual_calls=0)
    domain=surface.buffer(roadbed_lateral_reach(vehicle)).difference(reservations.difference(surface))
    options=[]
    for scale in (1.05,1.125,1.25):
        radius=scale*vehicle['minimumRearAxleRadiusM']
        families=chain(((('CCC',*kind),seed) for kind,seed in three_arc_seeds(entry,exit,radius)),
                       ((('CSC',*kind),seed) for kind,seed in arc_line_arc_seeds(entry,exit,radius)))
        for kind,seed in families:
            if not same_island_winding(seed,source_route,surface):continue
            outside=seed.buffer(vehicle['widthM']*.5).difference(domain).area
            options.append((outside,seed.length,radius,kind,seed))
    options=sorted(options,key=lambda row:row[:2])[:maximum_starts]
    calls,trials=0,[]
    for index,(_,_,radius,kind,seed) in enumerate(options):
        budget=(maximum_calls-calls)//(len(options)-index)
        if budget<1:break
        candidate,note=construct_fitted_roadbed(entry,exit,surface,reservations,seed,vehicle,error,maximum_calls=budget)
        calls+=note['residual_calls']
        if candidate is not None and not same_island_winding(candidate['path'],source_route,surface):
            candidate=None;note=dict(note,accepted=False,reason='return_turn_changes_source_winding')
        trials.append(dict(radius_m=radius,kind=kind,**note))
        if candidate is not None:
            report=dict(candidate['report'],seed_radius_m=radius,seed_kind=kind,residual_calls=calls,
                        maximum_residual_calls=maximum_calls,return_turn_trials=trials)
            return dict(candidate,report=report),report
    return None,dict(reason='no_clear_return_roadbed',residual_calls=calls,
                     maximum_residual_calls=maximum_calls,trials=trials)
