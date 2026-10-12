from pathlib import Path
import hashlib,json
import before_vehicle_sweep
import transport_corridor_experiment as corridors
import transport_junction_turn_experiment as junctions
import transport_junction_turns as turns
import transport_turn_paths as paths

root=Path('build/experiments/transport')
output=root/'swept-hulls'
vehicle=json.loads(Path('test/experiments/transport/transport_network_recipes.json').read_text())['vehicleProfiles']['compact']
current=turns.turn_body_check
new_reports={'corridors':[],'junctions':[]}
old_reports={'corridors':[],'junctions':[]}
comparison=[]
selected=[]
def decisions(report):
    return {(module['module'],turn['entry'],turn['exit']):turn['accepted']
            for module in report['modules'] for turn in module['turns']}
for name in ('Wien','ZuerichHauptbahnhof','BaselBadischerBahnhof'):
    for kind,experiment,limit in (('corridors',corridors,96),('junctions',junctions,48)):
        turns.turn_body_check=paths.turn_body_check=before_vehicle_sweep.turn_body_check
        before,_=experiment.survey(root,name,vehicle,limit)
        turns.turn_body_check=paths.turn_body_check=current
        after,views=experiment.survey(root,name,vehicle,limit)
        old_reports[kind].append(before)
        new_reports[kind].append(after)
        previous,next_decisions=decisions(before),decisions(after)
        assert previous.keys()==next_decisions.keys(),(name,kind,'changed request set')
        lost=[key for key,valid in previous.items() if valid and not next_decisions[key]]
        gains=[key for key,valid in next_decisions.items() if valid and not previous[key]]
        record=dict(place=name,kind=kind,before=before['counts'],after=after['counts'],
                    losses=lost,gains=gains,before_ms=before['total_ms'],after_ms=after['total_ms'])
        comparison.append(record)
        print(json.dumps(record),flush=True)
        gained={key[0] for key in gains}
        selected.extend(view for view in views if view[1] in gained)
        assert not lost,(name,kind,lost)
report=dict(comparison=comparison,vehicle_profile=vehicle,scope='Matched 96 corridors and 48 junctions per place; no full-network traffic proof.',
            implementation_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest()
                                   for p in Path('test/experiments/transport').glob('transport*.py')},
            source_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()
                           for folder in ('directed-graphs','directed-surfaces')
                           for p in (root/folder).glob('*') if p.suffix in ('.npz','.json')},
            baseline_vehicle_sha256=hashlib.sha256((output/'before_vehicle_sweep.py').read_bytes()).hexdigest())
(output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n')
for kind in new_reports:
    (output/f'{kind}.json').write_text(json.dumps(dict(places=new_reports[kind]),indent=2)+'\n')
    (output/f'before-{kind}.json').write_text(json.dumps(dict(places=old_reports[kind]),indent=2)+'\n')
junctions.render(Path('build/shots/experiments/transport/swept-hull-routes.png'),selected[:6],
                 title='Continuous swept hulls: matched old/new requests; newly usable vehicle routes')
