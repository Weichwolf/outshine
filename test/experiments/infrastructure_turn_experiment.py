#!/usr/bin/env python3
"""Compare bounded G1/G2 turns headlessly; render only explicitly selected recipes."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely

from infrastructure_clothoid_turns import fit_symmetric_turns,sample_symmetric_turns
from infrastructure_flat_plan import examples,plan
from infrastructure_flat_surfaces import physical_plan
from infrastructure_turn_paths import clearance_turns


def cases():
    recipes = examples()
    specifications = [('T',(-8,0),(0,0),(0,8)),('X',(0,-8),(0,0),(8,0)),
                      ('bend',(2,0),(10,0),(10,8)),
                      ('acute_merge',(-20,0),(0,0),(20,2)),
                      ('courtyard_loop',(2,0),(20,0),(20,18))]
    unequal = [dict(r,width=6 if i==0 else 4) for i,r in enumerate(recipes['T'])]
    recipes['unequal_T'] = unequal
    specifications.append(('unequal_T',(-8,0),(0,0),(0,8)))
    return [(name,recipes[name],*map(lambda p:np.array(p,dtype=float),(begin,node,end)))
            for name,begin,node,end in specifications]


def road_surface(source,configuration):
    roads,graphs,_ = plan(source,configuration,{})
    widths = np.array([r['width'] for r in roads])
    for graph in graphs.values():
        graph['half_widths'] = np.maximum.reduceat(widths[graph['owner_sources']],graph['owner_offsets'][:-1])*.5
    return physical_plan(list(graphs.values()),.001,'cuts',2)[0]


def verify_limits(configuration):
    source = examples()['T']
    narrow = road_surface([dict(r,width=1.4) for r in source],configuration)
    ordinary = road_surface(source,configuration)
    states = []
    for name,surface,end in (('insufficient_width',narrow,(0,8)),('insufficient_approach',ordinary,(0,3))):
        paths,reports = clearance_turns(np.zeros(2),np.array([-8.,0]),np.array(end),surface)
        assert not any(r['fits_surface'] for r in reports),(name,reports)
        states.append(dict(case=name,feasible=0))
    for angle in (20,135,270):
        theta = np.deg2rad(angle)
        rotation = np.array([[np.cos(theta),-np.sin(theta)],[np.sin(theta),np.cos(theta)]])
        transform = lambda xy:xy@rotation.T+np.array([100.,-50.])
        surface = shapely.transform(np.array([ordinary],dtype=object),transform)[0]
        node,begin,end = transform(np.array([[0.,0],[-8,0],[0,8]]))
        _,reports = clearance_turns(node,begin,end,surface)
        assert any(r['fits_surface'] for r in reports),(angle,reports)
        states.append(dict(rotation_deg=angle,feasible=sum(r['fits_surface'] for r in reports)))
    return states


def verify_math():
    rng = np.random.default_rng(2343)
    count = 1000
    heading = rng.uniform(-np.pi,np.pi,count)
    delta = rng.uniform(-np.pi*.9,np.pi*.9,count)
    trim = rng.uniform(2,40,count)
    start = time.perf_counter()
    fit = fit_symmetric_turns(rng.uniform(-100,100,(count,2)),trim,heading,heading+delta)
    fit_ms = (time.perf_counter()-start)*1000
    xy,tangent,curvature = sample_symmetric_turns(fit,np.linspace(0,1,33))
    assert np.all(fit['valid'])
    assert np.max(np.linalg.norm(xy[:,-1]-fit['end'],axis=1))<1e-7
    assert np.max(np.abs(curvature[:,[0,-1]]))==0
    assert np.allclose(tangent[:,-1],heading+fit['delta'],atol=1e-12,rtol=0)
    return dict(curves=count,fit_ms=fit_ms,maximum_closure_error_m=float(fit['closure_error_m'].max()),
                maximum_endpoint_curvature_per_m=0.,quadrature_orders=[16,64])


def render(path,products):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection
    fig,axes = plt.subplots(2,3,figsize=(15,10),constrained_layout=True)
    for ax,(name,surface,variants) in zip(axes.ravel(),products):
        rings = [shapely.get_coordinates(p.exterior) for p in shapely.get_parts(surface)]
        ax.add_collection(PolyCollection(rings,facecolors='#c7cdc3',edgecolors='#596a5a',linewidths=.5))
        chosen = []
        for profile,color in (('G1','#b28548'),('G2','#167dba')):
            paths,reports = variants[profile]
            feasible = [i for i,r in enumerate(reports) if r['fits_surface']]
            if feasible:
                best = min(feasible,key=lambda i:reports[i]['length_m'])
                xy = shapely.get_coordinates(paths[best])
                r = reports[best]['minimum_radius_m']
                ax.plot(*xy.T,color=color,linewidth=1.5,label=f'{profile}: R >= {r:.1f} m' if r else profile)
                chosen.append((xy,color))
        if chosen:
            xy = np.concatenate([p for p,c in chosen])
            low,high = xy.min(axis=0)-4,xy.max(axis=0)+4
            ax.set(xlim=(low[0],high[0]),ylim=(low[1],high[1]))
            ax.legend(fontsize=8)
        else:
            ax.autoscale()
        ax.set(aspect='equal',title=name,facecolor='#f1f0e8',xlabel='East [m]',ylabel='North [m]')
    fig.suptitle('Flat road turns: 1.8 m clearance width, minimum radius 4 m; lanes and vehicle length remain open')
    path.parent.mkdir(parents=True,exist_ok=True)
    fig.savefig(path,dpi=140)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--shots',type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    configuration = json.loads(Path(__file__).with_name('infrastructure_network_recipes.json').read_text())
    results,products = [],[]
    for name,source,begin,node,end in cases():
        surface = road_surface(source,configuration)
        variants = {}
        for profile in ('G1','G2'):
            start = time.perf_counter()
            paths,reports = clearance_turns(node,begin,end,surface,profile)
            elapsed = (time.perf_counter()-start)*1000
            variants[profile] = paths,reports
            feasible = sum(r['fits_surface'] for r in reports)
            results.append(dict(case=name,profile=profile,feasible=feasible,solve_ms=elapsed,candidates=reports))
            assert feasible,(name,profile,reports)
        products.append((name,surface,variants))
    report = dict(math=verify_math(),limits=verify_limits(configuration),recipes=results,
                  scope='Straight approaches and clearance width; lanes, swept vehicle bodies and curved approaches remain open.')
    report['implementation_sha256'] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
        (Path(__file__),*(Path(__file__).with_name(n) for n in ('infrastructure_clothoids.py',
         'infrastructure_clothoid_turns.py','infrastructure_turn_paths.py')))}
    (args.output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n')
    if args.shots is not None:
        render(args.shots/'flat-turn-recipes.png',products)
    print(json.dumps(dict(math=report['math'],recipes=[{k:r[k] for k in
          ('case','profile','feasible','solve_ms')} for r in results])))


if __name__=='__main__':
    main()
