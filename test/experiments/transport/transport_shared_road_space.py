"""Retry once on constructed road space, then check the full retained request set."""

import json

from transport_directed_routes import directed_module_routes
from transport_junction_surfaces import same_island_winding
from transport_turn_roadbed_experiment import reservations
from transport_turn_roadbeds import roadbed_site_check,compose_roadbeds
import transport_coupled_roadbed_experiment as coupled


def construct_shared_space(build,network,trial,owner,sections,affected,region,buildings,section_index,required,cache=None,*,baseline_required=None):
    initial=required if baseline_required is None else baseline_required
    assert initial<=required,'baseline source requests are not retained'
    product=build(network,trial,owner,sections,affected,region,buildings,section_index,initial,cache)
    if product['unresolved'] and not product['planned'].equals(product['base']):
        poses=product['poses'];base=product['base']
        routes,_=directed_module_routes(trial.graph,poses,trial.shape(owner),trial.tree)
        reserved=buildings.union(reservations(trial,region).difference(base))
        remaining=[];added=0
        for row in product['unresolved']:
            pair=tuple(row['pair']);entry,exit=(poses[i] for i in pair)
            candidate,_=coupled.construct_route_roadbed(entry,exit,product['planned'],reserved,routes[pair],
                                          trial.vehicle,trial.curbs['arc_error_m'])
            reason=None
            if candidate is None:reason='no_constructive_shared_space_candidate'
            elif not same_island_winding(candidate['path'],routes[pair],base):reason='source_winding_changed'
            elif not candidate['roadbed'].is_empty:
                _,_,reason=roadbed_site_check(candidate['roadbed'],base,reserved,trial.vehicle,trial.curbs['arc_error_m'])
            if reason is None:
                supports=[*[c['candidate']['roadbed'] for c in product['curves']],candidate['roadbed']]
                reason=coupled.coupled_addition_rejection(base,trial.shape(owner),supports,reserved,
                                                         section_index,trial,owner,affected)
            if reason is not None:
                remaining.append(row|dict(shared_space_rejection=reason));continue
            product['curves'].append(dict(pair=pair,candidate=candidate,existing_surface=candidate['roadbed'].is_empty,
                                         source_route=routes[pair]))
            _,_,product['planned'],product['physical'],product['quantum']=compose_roadbeds(base,trial.shape(owner),supports)
            added+=1
        product['unresolved']=remaining
        print(json.dumps(dict(module=owner,shared_space_gains=added,remaining=len(remaining))),flush=True)
    proved=coupled.request_keys(product['poses'],[c['pair'] for c in product['curves']])
    assert required<=proved,'previous routes lost after the shared-space retry'
    return product
