#!/usr/bin/env python3
"""Compare clothoid transition lengths on offset left/right lane turns."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely

from infrastructure_clothoid_turns import fit_symmetric_turns,sample_symmetric_turns
from infrastructure_flat_plan import examples
from infrastructure_turn_experiment import road_surface
from infrastructure_turn_paths import clearance_turns
from infrastructure_building_clearance import plot_geometry
from infrastructure_vehicle_sweep import body_outlines


def lane_poses(incoming,outgoing,approach,lateral_offset):
    incoming,outgoing = np.asarray(incoming,dtype=float),np.asarray(outgoing,dtype=float)
    right = lambda v:np.array([v[1],-v[0]])
    begin,end = -approach*incoming+lateral_offset*right(incoming),approach*outgoing+lateral_offset*right(outgoing)
    tangent = np.c_[incoming,outgoing]
    station = np.linalg.solve(tangent,end-begin)
    node = begin+station[0]*incoming
    if station[0]<=0 or station[1]<=0:
        raise ValueError('lane turn has no forward tangent intersection')
    return node,begin,end


def verify_transitions():
    rng = np.random.default_rng(2343)
    count = 2000
    initial = rng.uniform(-np.pi,np.pi,count)
    final = initial+rng.uniform(-.9*np.pi,.9*np.pi,count)
    transitions = rng.uniform(.0625,.5,count)
    start = time.perf_counter()
    fit = fit_symmetric_turns(rng.uniform(-100,100,(count,2)),rng.uniform(2,40,count),
                              initial,final,transition_fraction=transitions)
    elapsed_ms = (time.perf_counter()-start)*1000
    xy,headings,curvature = sample_symmetric_turns(fit,np.linspace(0,1,65))
    assert np.all(fit['valid'])
    assert np.max(np.linalg.norm(xy[:,-1]-fit['end'],axis=1))<1e-7
    assert np.max(np.abs(curvature[:,[0,-1]]))==0
    assert np.max(np.abs(headings[:,-1]-initial-fit['delta']))<1e-12
    return dict(curves=count,fit_ms=elapsed_ms,maximum_closure_error_m=float(fit['closure_error_m'].max()),
                minimum_transition_fraction=float(transitions.min()))


def render(path,products,vehicle):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection
    fig,axes = plt.subplots(2,3,figsize=(15,10),constrained_layout=True)
    for ax,(name,surface,node,begin,end,paths,reports,best) in zip(axes.ravel(),products):
        plot_geometry(ax,[surface],'#c7cdc3')
        if best is not None:
            report = reports[best]
            xy = shapely.get_coordinates(paths[best])
            ax.plot(*xy.T,color='#167dba',linewidth=1.5)
            incoming,outgoing = node-begin,end-node
            trim = min(np.linalg.norm(incoming),np.linalg.norm(outgoing))*report['factor']
            fit = fit_symmetric_turns(node,np.array([trim]),
                np.array([np.arctan2(incoming[1],incoming[0])]),
                np.array([np.arctan2(outgoing[1],outgoing[0])]),
                transition_fraction=report['transition_fraction'])
            positions,angles,_ = sample_symmetric_turns(fit,np.linspace(0,1,9))
            bodies = body_outlines(positions[0],angles[0],vehicle['widthM']*.5,
                                   vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
            ax.add_collection(PolyCollection(bodies,facecolors='#167dba22',edgecolors='#167dba',linewidths=.4))
            title = f'{name}: R >= {report["minimum_radius_m"]:.1f} m; transition {report["transition_fraction"]:g}'
        else:
            title = f'{name}: no valid connection'
        ax.set(xlim=(-13,13),ylim=(-13,13),aspect='equal',facecolor='#f1f0e8',
               xlabel='East [m]',ylabel='North [m]',title=title)
    fig.suptitle('Offset lane turns; complete 1.8 x 4.4 m rigid vehicle clearance; junction traffic control remains open')
    path.parent.mkdir(parents=True,exist_ok=True)
    fig.savefig(path,dpi=140)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--shots',type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    configuration = Path(__file__).with_name('infrastructure_network_recipes.json')
    recipes = json.loads(configuration.read_text())
    vehicle = recipes['vehicleProfiles']['compact']
    source = [dict(r,width=6.) for r in examples()['X']]
    surface = road_surface(source,recipes)
    products,results = [],[]
    variants = (('east-left',(1,0),(0,1)),('east-right',(1,0),(0,-1)),
                ('north-left',(0,1),(-1,0)),('north-right',(0,1),(1,0)),
                ('west-left',(-1,0),(0,-1)),('west-right',(-1,0),(0,1)))
    for name,incoming,outgoing in variants:
        node,begin,end = lane_poses(incoming,outgoing,8,1.5)
        paths,reports = [],[]
        began = time.perf_counter()
        for transition in (.5,.25,.125):
            trial,notes = clearance_turns(node,begin,end,surface,vehicle=vehicle,transition_fraction=transition)
            paths.extend(trial);reports.extend(notes)
        elapsed_ms = (time.perf_counter()-began)*1000
        valid = [i for i,r in enumerate(reports) if r['fits_surface']]
        best = max(valid,key=lambda i:(reports[i]['transition_fraction'],-reports[i]['length_m'])) if valid else None
        assert best is not None,(name,reports)
        results.append(dict(case=name,solve_ms=elapsed_ms,feasible=len(valid),chosen=reports[best],candidates=reports))
        products.append((name,surface,node,begin,end,paths,reports,best))
    report = dict(math=verify_transitions(),road_width_m=6,lane_offset_m=1.5,vehicle_profile=vehicle,cases=results,
                  scope='Constructed straight two-way approaches; actual source lane layouts and junction control remain open.')
    report['implementation_sha256'] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
        (Path(__file__),configuration,*(Path(__file__).with_name(n) for n in
          ('infrastructure_clothoid_turns.py','infrastructure_turn_paths.py','infrastructure_vehicle_sweep.py')))}
    (args.output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n')
    if args.shots is not None:
        render(args.shots/'flat-lane-turns.png',products,vehicle)
    print(json.dumps([{k:r[k] for k in ('case','solve_ms','feasible','chosen')} for r in results]))


if __name__=='__main__':
    main()
