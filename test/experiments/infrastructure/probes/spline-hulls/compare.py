from pathlib import Path
import hashlib,json
import spline_hulls as trial
import infrastructure_spline_routes as source
import infrastructure_corridor_experiment as corridors
import infrastructure_junction_turn_experiment as junctions
root=Path('build/experiments/infrastructure')
output=root/'spline-hulls'
vehicle=json.loads(Path('test/experiments/infrastructure/infrastructure_network_recipes.json').read_text())['vehicleProfiles']['compact']
source.spline_route=trial.spline_route
results={'corridors':[],'junctions':[]};comparison=[];selected=[]
def decisions(report):
    return {(m['module'],t['entry'],t['exit']):t['accepted'] for m in report['modules'] for t in m['turns']}
for kind,experiment,limit in (('corridors',corridors,96),('junctions',junctions,48)):
    baseline={r['place']:r for r in json.loads((root/'swept-hulls'/f'{kind}.json').read_text())['places']}
    for name,before in baseline.items():
        after,views=experiment.survey(root,name,vehicle,limit)
        results[kind].append(after)
        old,new=decisions(before),decisions(after)
        assert old.keys()==new.keys()
        lost=[key for key,valid in old.items() if valid and not new[key]]
        gained=[key for key,valid in new.items() if valid and not old[key]]
        record=dict(place=name,kind=kind,before=before['counts'],after=after['counts'],losses=lost,gains=gained,
                    before_ms=before['total_ms'],after_ms=after['total_ms'])
        comparison.append(record);print(json.dumps(record),flush=True)
        selected.extend(v for v in views if v[1] in {key[0] for key in gained})
        assert not lost,(name,kind,lost)
(output/'comparison.json').write_text(json.dumps(dict(comparison=comparison,prototype_sha256=hashlib.sha256((output/'spline_hulls.py').read_bytes()).hexdigest()),indent=2)+'\n')
for kind in results:
    (output/f'{kind}.json').write_text(json.dumps(dict(places=results[kind]),indent=2)+'\n')
if selected:
    junctions.render(Path('build/shots/experiments/infrastructure/spline-hull-routes.png'),selected[:6],title='Adaptive spline body hulls: newly feasible fixed-port routes')
