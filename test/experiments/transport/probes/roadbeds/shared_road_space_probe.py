"""Retry missing routes once on jointly constructed space without cumulative widening."""

import json
from pathlib import Path
import runpy
import sys

import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import transport_coupled_roadbed_experiment as coupled
from transport_directed_routes import directed_module_routes
from transport_junction_surfaces import same_island_winding
from transport_turn_roadbed_experiment import reservations
from transport_turn_roadbeds import roadbed_site_check,compose_roadbeds

original=coupled.construct_module


def shared_space(network,trial,owner,sections,affected,region,buildings,section_index,required,cache=None):
    product=original(network,trial,owner,sections,affected,region,buildings,section_index,required,cache)
    if not product['unresolved'] or product['planned'].equals(product['base']):return product
    poses=product['poses'];base=product['base']
    routes,_=directed_module_routes(trial.graph,poses,trial.shape(owner),trial.tree)
    reserved=buildings.union(reservations(trial,region).difference(base))
    remaining=[];added=0
    for row in product['unresolved']:
        pair=tuple(row['pair']);entry,exit=(poses[i] for i in pair)
        candidate,trials=coupled.construct_route_roadbed(entry,exit,product['planned'],reserved,routes[pair],
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
    return product


coupled.construct_module=shared_space
state=runpy.run_path(str(Path(__file__).with_name('joint_cut_recipe_probe.py')),run_name='__main__')
report=state['report'];output=state['args'].output
sources=coupled.implementation_files(Path(__file__))
report['implementation_sha256'].update({str(p.relative_to(Path.cwd())):coupled.digest(p) for p in sources})
report['experimental_recipe']+=' Missing routes retried once on constructed public space; widening still bounded by the original floor.'
(output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda obj:obj.tolist())+'\n')
