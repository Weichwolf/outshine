#!/usr/bin/env python3
"""Survey real flat infrastructure windows; retain compact results and failing inputs."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import resource
import sys
import time

import numpy as np
import shapely

from infrastructure_flat_plan import plan
from infrastructure_flat_surface_network import IMPLEMENTATION_SHA256
from infrastructure_flat_surfaces import solve
from infrastructure_network import save_inputs,transport_widths
from infrastructure_world_samples import site_roads,window_roads
from infrastructure_travel import compile_travel


def examine(transport,recipes,widths):
    began = time.perf_counter()
    cpu = resource.getrusage(resource.RUSAGE_SELF)
    accepted,graphs,usage = plan(transport,recipes,widths)
    travel = compile_travel(accepted,graphs,recipes['travelRecipes'])
    report = dict(source_axes=len(transport),accepted_axes=len(accepted),source_states=usage['states'],
                  unused_reasons=usage['reasons'],planning_ms=usage['elapsed_ms'],
                  unused_sources=[[u['source'],u['id'],u['reason']] for u in usage['usage'] if u['status']=='not_used'])
    report['travel'] = travel
    if not accepted:
        report.update(status='no_ground_assets',elapsed_ms=(time.perf_counter()-began)*1000)
        return report
    source_width = np.array([r['width'] for r in accepted])
    for graph in graphs.values():
        graph['half_widths'] = np.maximum.reduceat(source_width[graph['owner_sources']],graph['owner_offsets'][:-1])*.5
    product,surfaces = solve(list(graphs.values()),junction_mode='cuts',corner_radius=2)
    report.update(surfaces)
    report['product_bytes'] = sum(array.nbytes for array in product.values())
    peak = resource.getrusage(resource.RUSAGE_SELF)
    report['process_peak_rss_bytes'] = peak.ru_maxrss*(1 if sys.platform=='darwin' else 1024)
    report['cpu_ms'] = (peak.ru_utime+peak.ru_stime-cpu.ru_utime-cpu.ru_stime)*1000
    report['status'] = 'solved' if not (surfaces['ports']['invalid_sections'] or surfaces['cuts']['unresolved_cuts'] or
                                       surfaces['cuts']['remaining_cut_conflicts']) else 'unresolved'
    report['elapsed_ms'] = (time.perf_counter()-began)*1000
    return report


def run(args):
    inventory = json.loads(args.inventory.read_text())
    widths = transport_widths()
    recipe_path = Path(__file__).with_name('infrastructure_network_recipes.json')
    recipes = json.loads(recipe_path.read_text())
    sites = [s for s in inventory['sites'] if not args.sites or s[0] in args.sites]
    if args.sites and set(args.sites)-{s[0] for s in sites}:
        raise ValueError('unknown survey site')
    offsets = (np.arange(args.side)-(args.side-1)*.5)*args.spacing
    centres = sorted([(x,y) for x in offsets for y in offsets],key=lambda p:p[0]**2+p[1]**2)
    args.output.mkdir(parents=True,exist_ok=True)
    implementation = dict(IMPLEMENTATION_SHA256)
    for path in [Path(__file__),Path(__file__).with_name('infrastructure_world_samples.py'),
                 Path(__file__).with_name('infrastructure_network_graph.py'),recipe_path,
                 Path(__file__).with_name('infrastructure_network.py'),
                 Path(__file__).with_name('infrastructure_travel.py'),
                 Path(__file__).with_name('infrastructure_building_clearance.py'),
                 Path('src/assets/world/vegetation.json')]:
        implementation[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    header = dict(inventory=str(args.inventory),inventory_sha256=hashlib.sha256(args.inventory.read_bytes()).hexdigest(),
                  implementation_sha256=implementation,sites=[s[0] for s in sites],radius_m=args.radius,
                  sample_centres_m=centres,scope='Flat level-zero geometry and source travel permissions; lanes, driving curves and heights remain open.')
    (args.output/'survey.json').write_text(json.dumps(header,indent=2)+'\n')
    counts,attempted = Counter(),0
    with (args.output/'results.jsonl').open('w') as output:
        for site in sites:
            began = time.perf_counter()
            roads,receipt = site_roads(inventory,args.tiles,site,widths)
            tree = shapely.STRtree([r['line'] for r in roads])
            decode_ms = (time.perf_counter()-began)*1000
            site_counts = Counter()
            for ordinal,centre in enumerate(centres):
                if args.limit is not None and attempted>=args.limit:
                    break
                name = f'{site[0]}-{ordinal:03d}'
                transport = window_roads(roads,tree,centre,args.radius)
                try:
                    report = examine(transport,recipes,widths)
                except (ValueError,AssertionError) as error:
                    report = dict(status='failed',source_axes=len(transport),error=str(error))
                report.update(case=name,site=site[0],centre_m=centre)
                output.write(json.dumps(report,separators=(',',':'))+'\n')
                output.flush()
                counts[report['status']] += 1
                site_counts[report['status']] += 1
                attempted += 1
                if report['status'] in ('failed','unresolved'):
                    failure=args.output/'failures'
                    failure.mkdir(exist_ok=True)
                    save_inputs(failure/f'{name}-input.json',dict(receipt,sample_centre_m=centre),transport)
            print(json.dumps(dict(site=site[0],cases=sum(site_counts.values()),states=dict(site_counts),
                                  decode_ms=round(decode_ms),elapsed_s=round(time.perf_counter()-began,2))),flush=True)
            if args.limit is not None and attempted>=args.limit:
                break
    summary = dict(cases=attempted,states=dict(counts),expected_cases=len(sites)*len(centres))
    (args.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary),flush=True)
    return bool(counts['failed'] or counts['unresolved'])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory',type=Path,default=Path('doc/references/data/osm/openfreemap-inventory-20261009.json'))
    parser.add_argument('--tiles',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--side',type=int,default=5)
    parser.add_argument('--spacing',type=float,default=400,help='sample spacing only; geometry is not put on a grid')
    parser.add_argument('--radius',type=float,default=180)
    parser.add_argument('--limit',type=int)
    parser.add_argument('--sites',nargs='*')
    args=parser.parse_args()
    if args.side<1 or (args.limit is not None and args.limit<1) or not all(np.isfinite(v) and v>0 for v in (args.spacing,args.radius)):
        parser.error('positive finite dimensions and counts required')
    raise SystemExit(run(args))


if __name__=='__main__':
    main()
