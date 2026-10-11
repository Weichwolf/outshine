#!/usr/bin/env python3
"""Rejected height-lattice hypothesis; retained as a reproducible comparison.

Whole-cell bindings invent constraints away from road surfaces. Traffic planning
uses the single-level vector experiments; this script does not feed native assets.
"""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely

from infrastructure_network import source_roads
from infrastructure_network_plan import bands_of,global_plan
from infrastructure_height_constraints import grid,constraints
from infrastructure_height_grid import solve
from infrastructure_height_samples import sample
from infrastructure_height_mesh import mesh,render


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--sources',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--place',default='Wien')
    parser.add_argument('--centre',type=float,nargs=2,default=(-350,-400))
    parser.add_argument('--extent',type=float,default=180)
    parser.add_argument('--spacing',type=float,default=10)
    parser.add_argument('--iterations',type=int,default=5000)
    args = parser.parse_args()
    if not all(np.isfinite(v) and v>0 for v in (args.extent,args.spacing)):
        parser.error('positive finite dimensions required')
    args.output.mkdir(parents=True,exist_ok=True)
    raw_path = args.input/f'{args.place}-input.json'
    raw = json.loads(raw_path.read_text())
    previous = next(r for r in json.loads((args.input/'network.json').read_text()) if r['place']==args.place)
    templates = json.loads(Path('src/assets/world/vegetation.json').read_text())['templates']
    rules = {r['kind']:r for t in templates for r in t.get('osm',[])
             if r.get('layer')=='streets' and r.get('widthM',0)>0}
    recipes = json.loads(Path(__file__).with_name('infrastructure_network_recipes.json').read_text())
    transport = [dict(line=shapely.LineString(r['coordinates']),width=r['width_m'],
                      properties=r['properties']) for r in raw['roads']]
    roads,normalization = source_roads(transport,recipes,{k:r['widthM'] for k,r in rules.items()})
    began = time.perf_counter()
    xy,cells = grid(args.centre,args.extent,args.spacing)
    box = shapely.box(*xy[0,0],*xy[-1,-1])
    bands = bands_of(roads,.001)
    original_tiers = sorted({r['tier'] for r in roads if r['line'].intersects(box)})
    roads = [dict(r,tier=(int(r['properties'].get('layer',
                 {'bridge':1,'tunnel':-1}.get(r['tier'][1],0))),'level')) for r in roads]
    transitions = [dict(t,tiers=[[t['traffic'],*tier]
                     for tier in sorted({roads[source]['tier'] for source in t['sources']})])
                   for t in previous['transitions']]
    plan = {tier:shape.intersection(box) for tier,shape in global_plan(roads,bands).items()}
    plan = {tier:shape for tier,shape in plan.items() if shape.area>1e-9}
    tiers,maximum,lower,upper,gaps,equal,contacts = constraints(
        roads,bands,plan,transitions,xy,cells,rules)
    dem,receipts = sample(args.sources,raw['receipt']['origin'],xy.reshape(-1,2))
    base = tiers.index((0,'level')) if (0,'level') in tiers else 0
    offsets = np.array([6*(i-base) for i in range(len(tiers))])
    target = dem.reshape(xy.shape[:2])[None,:,:]+offsets[:,None,None]
    setup_ms = (time.perf_counter()-began)*1000
    fitted,report = solve(target,args.spacing,maximum,lower,upper,gaps,equal,args.iterations)
    began = time.perf_counter()
    products,mesh_report = mesh(xy,fitted,tiers,plan,maximum,args.spacing)
    mesh_ms = (time.perf_counter()-began)*1000
    complete = report['complete'] and all(m['maximum_face_grade_error']<=2e-5 and
                    m['area_error_m2']<=1e-7 for m in mesh_report)
    status = '2_5d' if complete else 'unresolved'
    image = args.output/status/f'{args.place}-height-field.png'
    render(image,args.place,products,complete)
    report.update(place=args.place,complete=bool(complete),centre_m=args.centre,extent_m=args.extent,
                  spacing_m=args.spacing,tiers=[list(t) for t in tiers],
                  original_structure_tiers=[list(t) for t in original_tiers],
                  source_sha256=hashlib.sha256(raw_path.read_bytes()).hexdigest(),
                  dem_sources=receipts,normalization=normalization,contacts=contacts,
                  guessed_initial_offsets_m=offsets.tolist(),setup_ms=setup_ms,mesh_ms=mesh_ms,
                  meshes=mesh_report,image=str(image),
                  scope='Continuous piecewise planar surfaces in one local window. '
                  'Offsets are prototype priors, not OSM layer heights. '
                  'Structures on the same explicit layer share a height field. '
                  'C1 curvature, portals and traffic rules remain unresolved.')
    (args.output/f'{args.place}-height-field.json').write_text(json.dumps(report,indent=2)+'\n')
    np.savez_compressed(args.output/f'{args.place}-height-field.npz',xy=xy,height=fitted,
                        maximum=maximum,lower=lower,upper=upper,gaps=gaps,equal=equal,
                        **{f'{i}_{k}':v for i,p in enumerate(products.values()) for k,v in p.items()})
    print(json.dumps(report))


if __name__=='__main__':
    main()
