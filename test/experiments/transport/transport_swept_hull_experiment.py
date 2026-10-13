#!/usr/bin/env python3
"""Audit continuous corner bounds, hidden obstacles and bounded analytic vehicle sweeps."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import shapely
from scipy.integrate import quad_vec

from transport_clothoids import fit_clothoids,sample_clothoids
from transport_clothoid_turns import fit_symmetric_turns,sample_symmetric_turns
from transport_curvature_paths import curvature_rate_bound,fit_curvature_paths,sample_curvature_paths
from transport_vehicle_sweep import body_outlines,swept_hull_check,turn_body_check


def verify_rates():
    rng = np.random.default_rng(2343)
    delta = np.r_[rng.uniform(-np.pi,np.pi,1024),0,1,-1,1,-1]
    amplitude = np.r_[rng.uniform(-np.pi,np.pi,1024),0,0,0,1e-15,-1e-15]
    length = rng.uniform(1,400,len(delta))
    bound = curvature_rate_bound(delta,amplitude,length)
    stations = np.linspace(0,1,1025)
    for i,(d,a) in enumerate(zip(delta,amplitude)):
        polynomial = np.array([16*a,-2*d-32*a,3*d+16*a,0,0])
        dense = np.max(np.abs(np.polyval(np.polyder(polynomial,2),stations)))/length[i]**2
        assert dense<=bound[i]+1e-12
        assert bound[i]-dense<1e-3/length[i]**2
    return dict(polynomials=len(delta),dense_stations=len(stations),analytic_rate_bound_verified=True)


def fitted_cases(count=32):
    rng = np.random.default_rng(2343)
    heading,delta = rng.uniform(-np.pi,np.pi,count),rng.uniform(-.3,.3,count)
    distance,lateral = rng.uniform(10,350,count),rng.uniform(-.05,.05,count)
    begin = rng.uniform(-100,100,(count,2))
    displacement = np.c_[np.cos(heading)-lateral*np.sin(heading),np.sin(heading)+lateral*np.cos(heading)]
    end = begin+distance[:,None]*displacement
    return [('G1',fit_clothoids(begin,end,heading,heading+delta),sample_clothoids),
            ('G2',fit_symmetric_turns(begin,distance*.5,heading,heading+delta,
                      transition_fraction=rng.choice([.125,.25,.5],count)),sample_symmetric_turns),
            ('G2-polynomial',fit_curvature_paths(begin,end,heading,heading+delta),sample_curvature_paths)]


def reference_heading(fit,index,profile):
    if profile=='G1':
        return lambda u:np.polyval([fit['bend'][index],fit['delta'][index]-fit['bend'][index],
                                   fit['heading'][index]+fit['initial'][index]],u)
    start,delta = fit['start_heading'][index],fit['delta'][index]
    if profile=='G2-polynomial':
        amplitude = fit['amplitude'][index]
        return lambda u:start+np.polyval([16*amplitude,-2*delta-32*amplitude,3*delta+16*amplitude,0,0],u)
    transition = fit['transition_fraction'][index]
    def heading(u):
        if u<=transition:
            progress = u*u/(2*transition*(1-transition))
        elif u<1-transition:
            progress = (u-transition*.5)/(1-transition)
        else:
            progress = 1-(1-u)**2/(2*transition*(1-transition))
        return start+delta*progress
    return heading


def verify_corner_envelopes(vehicle):
    radius = np.hypot(max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']),vehicle['widthM']*.5)
    stations,local = np.linspace(0,1,257),np.linspace(0,1,33)[None,:,None,None]
    results = []
    for profile,fit,sampler in fitted_cases():
        assert np.all(fit['valid'])
        xy,heading = sampler(fit,stations)[:2]
        bodies = body_outlines(xy.reshape(-1,2),heading.ravel(),vehicle['widthM']*.5,
                               vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']).reshape(len(xy),257,4,2)
        curvature,rate = fit['maximum_curvature_per_m'],fit['maximum_curvature_rate_per_m2']
        bound = (curvature+radius*(rate+curvature**2))*fit['length_m']**2/(8*8**2)
        maximum_ratio = 0.
        for first in range(0,256,32):
            interpolated = bodies[:,first,None]*(1-local)+bodies[:,first+32,None]*local
            deviation = np.linalg.norm(bodies[:,first:first+33]-interpolated,axis=-1).max(axis=(1,2))
            assert np.all(deviation<=bound+1e-9),(profile,deviation,bound)
            maximum_ratio = max(maximum_ratio,float(np.max(deviation/bound)))
        numerical_error = 0.
        for index in (0,int(np.argmax(rate))):
            angle = reference_heading(fit,index,profile)
            for u in (.13,.37,.83,1.):
                integral,_ = quad_vec(lambda t:np.array([np.cos(angle(t)),np.sin(angle(t))]),
                                      0,u,epsabs=1e-12,epsrel=1e-12)
                expected = fit['start'][index]+fit['length_m'][index]*integral
                actual = sampler(fit,np.array([u]))[0][index,0]
                numerical_error = max(numerical_error,float(np.linalg.norm(expected-actual)))
        assert numerical_error<1e-8,(profile,numerical_error)
        results.append(dict(profile=profile,curves=len(xy),intervals_per_curve=8,
                            maximum_deviation_to_bound_ratio=maximum_ratio,
                            maximum_independent_integration_error_m=numerical_error))
    return results


def verify_obstacles(vehicle):
    def straight(owners,stations):
        return np.c_[20*stations,np.zeros(len(stations))],np.zeros(len(stations))
    floor = shapely.difference(shapely.box(-5,-3,25,3),shapely.box(9,-.2,11,.2))
    endpoints = body_outlines(np.array([[0.,0.],[20.,0.]]),np.zeros(2),vehicle['widthM']*.5,
                             vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
    assert np.all(shapely.covers(floor,shapely.polygons(endpoints)))
    blocked = swept_hull_check(floor,straight,[0.],vehicle)
    assert not blocked['fits_surface'] and blocked['outside_poses']>0
    budget = swept_hull_check(floor,straight,[0.],vehicle,maximum_poses=2)
    assert not budget['fits_surface'] and budget['reason']=='vehicle_sweep_budget_exhausted'
    def circular(owners,stations):
        return np.c_[10*np.sin(stations),10*(1-np.cos(stations))],stations
    xy,heading = circular(None,np.linspace(0,1,1025))
    bodies = shapely.polygons(body_outlines(xy,heading,vehicle['widthM']*.5,
                                            vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']))
    floor = shapely.buffer(shapely.union_all(bodies),.03)
    radius = np.hypot(max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']),vehicle['widthM']*.5)
    acceleration = 10+radius
    adaptive = swept_hull_check(floor,circular,[acceleration],vehicle,error=.25)
    assert adaptive['fits_surface'] and adaptive['hull_intervals']>np.ceil(np.sqrt(acceleration/(8*.25)))
    assert adaptive['sampled_poses']<=4096 and adaptive['sweep_deviation_bound_m']<.03
    return dict(hidden_hole_rejected=blocked,refinement_budget_enforced=budget,tight_curve_refined=adaptive)


def verify_long_turns(vehicle):
    results = []
    begin,end = np.array([[-175.,0.]]),np.array([[175.,0.]])
    straight = [('G1',fit_clothoids(begin,end,np.zeros(1),np.zeros(1))),
                ('G2',fit_symmetric_turns(np.zeros(2),np.array([175.]),np.zeros(1),np.zeros(1))),
                ('G2-polynomial',fit_curvature_paths(begin,end,np.zeros(1),np.zeros(1)))]
    floor = shapely.box(-180,-5,180,20)
    for profile,fit in straight:
        report,_ = turn_body_check(floor,fit,begin[0],end[0],profile,vehicle,maximum_poses=2)
        assert report['fits_surface'] and report['sampled_poses']==2
        results.append(dict(profile=profile,straight=report))
    fit = fit_symmetric_turns(np.zeros(2),np.array([175.]),np.zeros(1),np.array([.07]))
    report,_ = turn_body_check(floor,fit,fit['start'][0],fit['end'][0],'G2',vehicle,maximum_poses=64)
    assert report['fits_surface'] and report['sampled_poses']<=64
    radius = np.hypot(max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']),vehicle['widthM']*.5)
    old_poses = int(np.ceil(fit['length_m'][0]*(1+fit['maximum_curvature_per_m'][0]*radius)/.05))+1
    assert old_poses>4096
    return dict(straight_profiles=results,shallow_curve_length_m=float(fit['length_m'][0]),
                first_order_required_poses=old_poses,swept_hulls=report)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args()
    configuration = Path(__file__).with_name('transport_network_recipes.json')
    vehicle = json.loads(configuration.read_text())['vehicleProfiles']['compact']
    report = dict(rates=verify_rates(),corners=verify_corner_envelopes(vehicle),
                  obstacles=verify_obstacles(vehicle),long_turns=verify_long_turns(vehicle),vehicle_profile=vehicle)
    names = ('transport_vehicle_sweep.py','transport_clothoids.py',
             'transport_clothoid_turns.py','transport_curvature_paths.py')
    report['implementation_sha256'] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
                                      (Path(__file__),configuration,*(Path(__file__).with_name(n) for n in names))}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(rates=report['rates'],corners=report['corners'],
                         long_turn=report['long_turns']['swept_hulls'],hidden_hole_rejected=True)))


if __name__=='__main__':
    main()
