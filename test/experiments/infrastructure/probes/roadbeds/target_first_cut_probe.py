"""Reject impossible target curves before rebuilding unchanged neighboring requests."""

import json
from pathlib import Path
import runpy
import sys

import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import infrastructure_coupled_roadbed_experiment as coupled
from infrastructure_directed_routes import directed_module_routes
from infrastructure_turn_roadbed_experiment import reservations

original_module=coupled.construct_module
original_headings=coupled.joint_heading_trials
phase=dict(proposal=None,tried=0,rejected=0)


def proposed_headings(network,module,request,maximum_candidates=64):
    try:
        for trial,changed,owners,cost in original_headings(network,module,request,maximum_candidates):
            phase['proposal']=(trial,module,request)
            yield trial,changed,owners,cost
    finally:phase['proposal']=None


def target_first_module(network,trial,owner,sections,affected,region,buildings,section_index,required,cache=None):
    proposal=phase['proposal']
    if proposal is not None and trial is proposal[0] and owner==proposal[1]:
        poses,_=trial.poses(owner,{})
        routes,_=directed_module_routes(trial.graph,poses,trial.shape(owner),trial.tree)
        pair=next((pair for pair in routes if next(iter(coupled.request_keys(poses,[pair])))==proposal[2]),None)
        assert pair is not None,'selected_request_changed'
        floor=coupled.motor_surface(shapely.union_all(list(routes.values())).envelope,
                   trial.graph,trial.tree,trial.mesh,trial.vehicle,trial.curbs['corner_radius_m'],trial.curbs['arc_error_m'])
        reserved=buildings.union(reservations(trial,region).difference(floor))
        if cache is None:cache=dict(contexts={},hits=0,misses=0)
        candidate,_,_=coupled.roadbed_candidate(poses[pair[0]],poses[pair[1]],routes[pair],floor,reserved,
                       trial.vehicle,trial.curbs['arc_error_m'],proposal[2] in required,cache)
        phase['tried']+=1
        if candidate is None:
            phase['rejected']+=1
            raise AssertionError('selected_request_has_no_candidate')
    return original_module(network,trial,owner,sections,affected,region,buildings,section_index,required,cache)


coupled.construct_module=target_first_module
coupled.joint_heading_trials=proposed_headings
try:
    state=runpy.run_path(str(Path(__file__).with_name('joint_cut_recipe_probe.py')),run_name='__main__')
except AssertionError:
    print(json.dumps({k:phase[k] for k in ('tried','rejected')}),flush=True)
    raise
report=state['report'];output=state['args'].output
sources=set(coupled.implementation_files(Path(__file__)))|set(coupled.implementation_files(Path(coupled.__file__)))
report['implementation_sha256'].update({str(p.relative_to(Path.cwd())):coupled.digest(p) for p in sources})
report['target_preflight']={k:phase[k] for k in ('tried','rejected')}
report['experimental_recipe']+=' Failed target curves reject a frame proposal before other requests; successful candidates are reused.'
(output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda obj:obj.tolist())+'\n')
print(json.dumps(report['target_preflight']),flush=True)
