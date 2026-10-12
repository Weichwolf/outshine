"""Check every physical module in a radius with frozen shared motor-lane frames."""

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

from transport_driving_network import DrivingNetwork
from transport_hermite_experiment import fit_module
from transport_junction_surfaces import mesh_surface_index
from transport_shared_frames import frame_key,accepted_routes


def window_modules(product,mesh,radius):
    faces = mesh[1].query(shapely.Point(0,0),predicate='dwithin',distance=radius)
    return np.unique(product['face_owners'][faces]).tolist()


def verify_window_selection():
    product = dict(vertices=np.array([[-2.,0.],[2.,0.],[2.,.2],[3.,3.],[4.,3.],[3.,4.]]),
                   indices=np.array([[0,1,2],[3,4,5]]),face_owners=np.array([7,11]))
    mesh = mesh_surface_index(product)
    assert window_modules(product,mesh,.1)==[7]
    assert window_modules(product,mesh,5)==[7,11]
    assert window_modules(product,mesh,2)==[7]
    return dict(crossing_triangle_selected=True,distant_module_excluded=True)


def classify_module(network,module):
    if module not in network.junctions and module not in network.corridors:
        return dict(module=module,reason='no_physical_interface',unused_ports=[])
    poses,unused = network.poses(module,{})
    incoming = sum(p['incoming'] for p in poses)
    sections = len({p['section'] for p in poses})
    reason = ('no_public_motor_ports' if not poses else
              'single_public_motor_section' if sections<2 else
              'no_opposed_public_motor_ports' if not incoming or incoming==len(poses) else None)
    return dict(module=module,reason=reason,ports=len(poses),sections=sections,
                incoming=incoming,outgoing=len(poses)-incoming,unused_ports=unused)


def route_comparison(previous,current):
    requests = lambda r:{(t['entry'],t['exit']) for t in r['turns']}
    assert requests(previous)==requests(current),'a previously requested route disappeared'
    assert accepted_routes(previous)<=accepted_routes(current),'a previously valid route disappeared'


def verify_route_comparison():
    row = lambda pairs:dict(turns=[dict(entry=a,exit=b,accepted=c) for a,b,c in pairs])
    previous = row([(0,1,True),(0,2,False)])
    route_comparison(previous,row([(0,1,True),(0,2,True)]))
    for current in (row([(0,1,False),(0,2,True)]),row([(0,1,True)])):
        try:
            route_comparison(previous,current)
        except AssertionError:
            continue
        raise AssertionError('route regression was accepted')
    return dict(lost_requests_rejected=True,lost_valid_routes_rejected=True)


def survey(root,reference,vehicle,radius,maximum_calls):
    started = time.perf_counter()
    usage = resource.getrusage(resource.RUSAGE_SELF)
    inputs = [root/folder/f'{reference["place"]}-{suffix}.{ext}' for folder,suffix in
              [('directed-graphs','flat-network'),('directed-surfaces','flat-surfaces')] for ext in ('npz','json')]
    hashes = {str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    assert hashes==reference['input_sha256'],'the source network changed since the previous cohort'
    network = DrivingNetwork(root,reference['place'],vehicle)
    network.frames = {frame_key(f):{k:np.array(f[k]) for k in ('position','heading')} for f in reference['frames']}
    selected = window_modules(network.product,network.mesh,radius)
    previous = {r['module']:r for r in reference['modules']}
    assert previous.keys()<=set(selected),'the larger window must contain the previous cohort'
    classifications,records,trials,paths = [],[],[],[]
    baseline,fit_ms,classification_ms = Counter(),0.,0.
    for index,module in enumerate(selected):
        began = time.perf_counter()
        row = classify_module(network,module)
        classification_ms += (time.perf_counter()-began)*1000
        classifications.append(row)
        if row['reason'] is None:
            before,_ = network.record(module)
            baseline.update(modules=1,connections=before['connections'],accepted=before['accepted'])
            began = time.perf_counter()
            (after,view),local = fit_module(network,module,maximum_calls)
            fit_ms += (time.perf_counter()-began)*1000
            records.append(after)
            trials.extend(dict(module=module,**t) for t in local)
            paths.append(dict(module=module,paths=[None if p is None else shapely.get_coordinates(p).tolist()
                                                   for p in view[3]]))
            if module in previous:
                route_comparison(previous[module],after)
        else:
            assert module not in previous,'a previously checked module became unclassified'
        if (index+1)%64==0:
            print(json.dumps(dict(place=reference['place'],physical_modules=index+1,
                  checked_modules=len(records),connections=sum(r['connections'] for r in records),
                  accepted=sum(r['accepted'] for r in records),elapsed_ms=(time.perf_counter()-started)*1000)),flush=True)
    assert len(classifications)==len(selected) and len({r['module'] for r in classifications})==len(selected)
    counts = dict(modules=len(records),connections=sum(r['connections'] for r in records),
                  accepted=sum(r['accepted'] for r in records))
    ended = resource.getrusage(resource.RUSAGE_SELF)
    return dict(place=reference['place'],radius_m=radius,physical_modules=len(selected),before=dict(baseline),after=counts,
                previous_modules_preserved=len(previous),classifications=classifications,modules=records,trials=trials,
                paths=paths,excluded_reasons=dict(Counter(r['reason'] for r in classifications if r['reason'])),
                classification_ms=classification_ms,fit_ms=fit_ms,total_ms=(time.perf_counter()-started)*1000,
                cpu_user_ms=(ended.ru_utime-usage.ru_utime)*1000,cpu_system_ms=(ended.ru_stime-usage.ru_stime)*1000,
                process_peak_rss_bytes=ended.ru_maxrss*(1 if sys.platform=='darwin' else 1024),
                input_sha256=hashes)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,default=Path('build/experiments/transport'))
    parser.add_argument('--reference',type=Path,default=Path('build/experiments/transport/boundary-optimized/hermite-routes.json'))
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--radius-m',type=float,default=1000.)
    parser.add_argument('--maximum-calls',type=int,default=1024)
    parser.add_argument('places',nargs='*')
    args = parser.parse_args()
    if not np.isfinite(args.radius_m) or args.radius_m<=0 or args.maximum_calls<1:
        parser.error('radius and residual budget must be positive')
    checks = {**verify_window_selection(),**verify_route_comparison()}
    reference = json.loads(args.reference.read_text())
    paths = [Path(__file__),*(Path(__file__).with_name(n) for n in reference['implementation_sha256'])]
    vehicle = json.loads(Path(__file__).with_name('transport_network_recipes.json').read_text())['vehicleProfiles']['compact']
    args.output.mkdir(parents=True,exist_ok=True)
    reports = []
    for name in args.places:
        source = next(p for p in reference['places'] if p['place']==name)
        report = survey(args.input,source,vehicle,args.radius_m,args.maximum_calls)
        reports.append(report)
        (args.output/f'{name}-routes.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps({k:v for k,v in report.items() if k not in
                        ('classifications','modules','trials','paths','input_sha256')}),flush=True)
    result = dict(checks=checks,places=reports,
                  implementation_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
                  reference_sha256=hashlib.sha256(args.reference.read_bytes()).hexdigest(),
                  scope='Every intersecting physical module classified; public ground motor routes only. Shared poses frozen.')
    (args.output/'window-routes.json').write_text(json.dumps(result,indent=2)+'\n')


if __name__=='__main__':
    main()
