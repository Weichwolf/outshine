#!/usr/bin/env python3
"""Check source-following G2 routes and exact straight vehicle extrusion."""

import argparse
import hashlib
import json
from pathlib import Path
import statistics
import time

import numpy as np
import shapely

from infrastructure_flat_corners import rounded_plan
from infrastructure_route_turns import source_route_connector
from infrastructure_vehicle_sweep import body_outlines,straight_body_check


def verify_prism(vehicle):
    begin,end = np.array([0.,0.]),np.array([500.,0.])
    floor = shapely.box(-5,-2,505,2)
    report,_ = straight_body_check(floor,begin,end,0.,vehicle)
    assert report['fits_surface'] and report['sampled_poses']==2
    blocked = shapely.difference(floor,shapely.box(249,-2,251,2))
    rejected,outlines = straight_body_check(blocked,begin,end,0.,vehicle)
    assert not rejected['fits_surface'] and rejected['outside_poses']==0
    endpoints = shapely.polygons(outlines)
    assert np.all(shapely.covers(blocked,endpoints)),'endpoint-only checking misses the middle gap'
    samples = body_outlines(np.c_[np.linspace(0,500,1001),np.zeros(1001)],np.zeros(1001),
                            vehicle['widthM']*.5,vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
    prism = shapely.convex_hull(shapely.MultiPoint(outlines.reshape(-1,2)))
    assert np.all(shapely.covers(prism,shapely.polygons(samples)))
    timings = {}
    for length in (1,10,100,1000):
        floor = shapely.box(-5,-2,length+5,2)
        times = []
        for _ in range(30):
            began = time.perf_counter()
            report,_ = straight_body_check(floor,begin,np.array([float(length),0]),0,vehicle)
            times.append((time.perf_counter()-began)*1000)
            assert report['fits_surface']
        timings[str(length)] = statistics.median(times)
    return dict(middle_gap_rejected=True,endpoint_tests_insufficient=True,
                exact_translation_sweep=True,median_ms_by_length_m=timings)


def verify_route(vehicle):
    points = np.array([[-12,0],[-12,12],[12,12],[12,0]],dtype=float)
    route = shapely.LineString(points)
    floor,_ = rounded_plan(shapely.buffer(route,4.,quad_segs=8),2.)
    entry,exit = dict(position=points[0],heading=np.array([0.,1.])),dict(position=points[-1],heading=np.array([0.,-1.]))
    path,report = source_route_connector(entry,exit,route,floor,vehicle)
    assert path is not None and report['vehicle_sweep']['fits_surface']
    assert not shapely.intersects(path,shapely.box(-7,0,7,7)),'the route must go around the island'
    blocked = shapely.difference(floor,shapely.box(-4,8,4,16))
    rejected,note = source_route_connector(entry,exit,route,blocked,vehicle)
    assert rejected is None
    return dict(island_avoided=True,body_gap_rejected=True,chosen=report,rejected_reason=note['reason'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args()
    configuration = Path(__file__).with_name('infrastructure_network_recipes.json')
    vehicle = json.loads(configuration.read_text())['vehicleProfiles']['compact']
    report = dict(prism=verify_prism(vehicle),route=verify_route(vehicle),vehicle_profile=vehicle)
    report['implementation_sha256'] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
        (Path(__file__),configuration,*(Path(__file__).with_name(n) for n in
          ('infrastructure_route_turns.py','infrastructure_junction_routes.py','infrastructure_junction_turns.py',
           'infrastructure_vehicle_sweep.py','infrastructure_curvature_paths.py')))}
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'routes.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(prism=report['prism'],route_island_avoided=report['route']['island_avoided'],
                          route_body_gap_rejected=report['route']['body_gap_rejected'])))


if __name__=='__main__':
    main()
