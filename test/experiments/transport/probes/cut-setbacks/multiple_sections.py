import json
import sys
from pathlib import Path

import numpy as np
import shapely

sys.path.insert(0,str(Path('test/experiments/transport').resolve()))
from transport_cut_setback_experiment import prepared_network,digest
from transport_cut_setbacks import fork_cut_plan,displaced_source_section,check_external_sections,route_signatures
from transport_hermite_experiment import fit_module


def partition(core,road,point,tangent,preserved):
    area = shapely.union_all([core,road])
    tangent = tangent/np.linalg.norm(tangent)
    normal = np.array([-tangent[1],tangent[0]])
    extent = np.linalg.norm(np.subtract(area.bounds[2:],area.bounds[:2]))+1
    ray = shapely.LineString([point-normal*extent,point+normal*extent])
    spans = [g for g in shapely.get_parts(shapely.intersection(area,ray)) if g.geom_type=='LineString' and g.distance(shapely.Point(point))<1e-7]
    assert len(spans)==1
    line = shapely.LineString(shapely.get_coordinates(spans[0])[[0,-1]])
    cuts = []
    for section in [line,*preserved]:
        ends = shapely.get_coordinates(section)[[0,-1]]
        direction = (ends[1]-ends[0])/section.length
        cuts.append(shapely.LineString(ends+np.array([-1e-6,1e-6])[:,None]*direction))
    segments = shapely.get_parts(shapely.union_all([shapely.boundary(area),*cuts]))
    pieces = shapely.get_parts(shapely.polygonize(segments))
    pieces = pieces[shapely.contains(area,shapely.point_on_surface(pieces))]
    overlaps = shapely.area(shapely.intersection(pieces,core))
    tolerance = 64*np.finfo(float).eps*max(1.,np.abs(area.bounds).max())*area.length
    new_core = shapely.union_all(pieces[overlaps>tolerance])
    new_road = shapely.union_all(pieces[overlaps<=tolerance])
    assert not new_core.is_empty and not new_road.is_empty
    assert shapely.difference(core,new_core).area<tolerance
    assert shapely.intersection(new_core,new_road).area<tolerance
    assert shapely.symmetric_difference(area,shapely.union_all([new_core,new_road])).area<tolerance
    shared = shapely.intersection(shapely.boundary(new_core),shapely.boundary(new_road))
    assert shapely.hausdorff_distance(shared,shapely.union_all([line,*preserved]))<1e-7
    return new_core,new_road,line


root = Path('build/experiments/transport')
name = 'BaselBadischerBahnhof'
baseline = json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
frames = json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
n = prepared_network(root,baseline,next(p['frames'] for p in frames['places'] if p['place']==name))
module = 1109
poses,_ = n.poses(module,{})
sections = {poses[i]['section'] for i in (50,55)}
affected = {module}|{p['corridor'] for p in poses if p['section'] in sections}
old = {r['module']:r for r in baseline['modules']}
old.update((m,fit_module(n,m,1024)[0][0]) for m in affected if m not in old)
results = []
for distance in (4.,8.,12.):
    trial = fork_cut_plan(n,module,sections)
    for section in sorted(sections):
        pose = next(p for p in poses if p['section']==section)
        point,tangent,station = displaced_source_section(n,pose,distance)
        corridor = pose['corridor']
        preserved = [p['line'] for p in trial.junctions[module] if p['corridor']==corridor and p['section']!=section]
        core,road,line = partition(trial.shape(module),trial.shape(corridor),point,tangent,preserved)
        trial.shapes[module],trial.shapes[corridor],trial.sections[section] = core,road,line
        for port in trial.junctions[module]:
            if port['section']==section:
                port['line'] = line
    check_external_sections(trial,affected)
    solved = {m:fit_module(trial,m,1024)[0] for m in sorted(affected)}
    requests = all(route_signatures(old[m],n.poses(m,{})[0])==route_signatures(solved[m][0],solved[m][1][2]) for m in affected)
    retained = all(route_signatures(old[m],n.poses(m,{})[0],True)<=route_signatures(solved[m][0],solved[m][1][2],True) for m in affected)
    row = dict(distance_m=distance,requests_preserved=requests,valid_routes_preserved=retained,
        before={m:[old[m]['accepted'],old[m]['connections']] for m in sorted(affected)},
        after={m:[solved[m][0]['accepted'],solved[m][0]['connections']] for m in sorted(affected)},
        target=next(t for t in solved[module][0]['turns'] if t['entry']==50 and t['exit']==55))
    print(json.dumps(row),flush=True)
    results.append(row)
Path('build/experiments/transport/cut-setbacks/multiple-sections-results.json').write_text(json.dumps(dict(results=results,
    prototype_sha256=digest(Path(__file__)),input_sha256=baseline['input_sha256'],
    reference_sha256=digest(root/'window-1000-body'/f'{name}-routes.json')),indent=2)+'\n')
