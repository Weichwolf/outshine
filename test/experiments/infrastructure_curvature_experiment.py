#!/usr/bin/env python3
"""Check G2 polynomial pose interpolation, analytic curvature and rigid body clearance."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely

from infrastructure_curvature_paths import curvature_extrema,fit_curvature_paths,sample_curvature_paths
from infrastructure_junction_turns import curvature_connector


def verify_math():
    rng = np.random.default_rng(2343)
    count = 2048
    heading = rng.uniform(-np.pi,np.pi,count)
    distance,lateral,delta = rng.uniform(10,40,count),rng.uniform(-2,2,count),rng.uniform(-.5,.5,count)
    start = rng.uniform(-1000,1000,(count,2))
    end = start+np.c_[distance*np.cos(heading)-lateral*np.sin(heading),
                      distance*np.sin(heading)+lateral*np.cos(heading)]
    began = time.perf_counter()
    fit = fit_curvature_paths(start,end,heading,heading+delta)
    elapsed_ms = (time.perf_counter()-began)*1000
    xy,angles,curvature = sample_curvature_paths(fit,np.linspace(0,1,257))
    assert np.all(fit['valid'])
    assert np.max(np.linalg.norm(xy[:,-1]-end,axis=1))<1e-7
    assert np.max(np.abs(curvature[:,[0,-1]]))==0
    assert np.max(np.abs(angles[:,-1]-heading-delta))<1e-12
    maximum = np.max(np.abs(curvature),axis=1)
    assert np.all(maximum<=fit['maximum_curvature_per_m']+1e-12)
    return dict(curves=count,fit_ms=elapsed_ms,maximum_closure_error_m=float(fit['closure_error_m'].max()),
                analytic_curvature_extrema=True,zero_endpoint_curvature=True)


def verify_body(vehicle):
    entry = dict(position=np.array([0.,0.]),heading=np.array([1.,0.]))
    exit = dict(position=np.array([20.,3.]),heading=np.array([1.,0.]))
    shape = shapely.box(-5,-5,25,8)
    path,report = curvature_connector(entry,exit,shape,vehicle,.025)
    assert path is not None and report['vehicle_sweep']['fits_surface']
    obstacle = shapely.buffer(path.interpolate(.5,normalized=True),.2)
    rejected,note = curvature_connector(entry,exit,shapely.difference(shape,obstacle),vehicle,.025)
    assert rejected is None and note['reason']=='polynomial_vehicle_body_outside_surface'
    return dict(offset_lane_connection=True,body_obstacle_rejected=True,chosen=report)


def verify_extrema():
    rng = np.random.default_rng(2343)
    delta = np.r_[rng.uniform(-np.pi,np.pi,2048),0,1,-1,1,-1]
    amplitude = np.r_[rng.uniform(-np.pi,np.pi,2048),0,0,0,1e-15,-1e-15]
    length = np.ones(len(delta))
    maximum = curvature_extrema(delta,amplitude,length)
    u = np.linspace(0,1,1025)
    dense = np.abs(u*(1-u)*(6*delta[:,None]+32*amplitude[:,None]*(1-2*u))).max(axis=1)
    assert np.all(dense<=maximum+1e-12)
    assert np.all(np.abs(dense-maximum)<1e-4)
    assert maximum[-5]==0 and np.allclose(maximum[-4:],1.5)
    return dict(coefficients=len(delta),degenerate_quadratic_roots=True,dense_samples=1025)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args()
    configuration = Path(__file__).with_name('infrastructure_network_recipes.json')
    vehicle = json.loads(configuration.read_text())['vehicleProfiles']['compact']
    report = dict(math=verify_math(),extrema=verify_extrema(),body=verify_body(vehicle),vehicle_profile=vehicle,
                  scope='G2 polynomial heading interpolation; no assertion of global optimality or full network routing.')
    report['implementation_sha256'] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
        (Path(__file__),configuration,*(Path(__file__).with_name(n) for n in
          ('infrastructure_curvature_paths.py','infrastructure_junction_turns.py','infrastructure_vehicle_sweep.py')))}
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'math.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report['math']|report['body']))


if __name__=='__main__':
    main()
