"""Verify Bernstein bounds and clamped source splines before real-network use."""

import argparse
import hashlib
import json
from math import comb
from pathlib import Path

import numpy as np
import shapely
from scipy.interpolate import BPoly

from infrastructure_spline_routes import (body_acceleration_bounds,curve_bounds,evaluate_controls,
                                        split_controls,spline_controls,spline_route)
from infrastructure_route_fairing import fair_route
from infrastructure_vehicle_sweep import body_outlines


def verify_fairing():
    rng = np.random.default_rng(2343)
    cases = 0
    for count in (2,3,4,5,16,66):
        points = np.cumsum(rng.normal(size=(count,2)),axis=0)
        second = np.diff(np.eye(count),n=2,axis=0)
        for weight in (0.,.1,1.,10.,100.):
            fitted = fair_route(points,weight)
            matrix = np.eye(count)+weight*np.einsum('ki,kj->ij',second,second)
            expected = points.copy()
            if count>2:
                rhs = points[1:-1]-matrix[1:-1,0,None]*points[0]-matrix[1:-1,-1,None]*points[-1]
                expected[1:-1] = np.linalg.solve(matrix[1:-1,1:-1],rhs)
            assert np.allclose(fitted,expected,atol=1e-10,rtol=0)
            assert np.array_equal(fitted[[0,-1]],points[[0,-1]])
            energy = lambda p:np.sum((p-points)**2)+weight*np.sum(np.diff(p,n=2,axis=0)**2)
            assert energy(fitted)<=energy(points)+1e-10
            rotation = np.array([[.6,-.8],[.8,.6]])
            transform = lambda p:np.einsum('ij,kj->ki',rotation,p)+[10000.,-20000.]
            assert np.allclose(fair_route(transform(points),weight),transform(fitted),atol=1e-8,rtol=0)
            cases += 1
    return dict(cases=cases,dense_solution_agrees=True,fixed_endpoints=True,
                objective_not_increased=True,rigid_transform_invariant=True)


def verify_batched_bounds():
    rng=np.random.default_rng(2343)
    cases=0
    for degree in (3,5,7):
        for count in (1,8,48,128):
            controls=np.cumsum(rng.normal(size=(count,degree+1,2)),axis=1)
            controls[0]=0
            minimum,_,actual=curve_bounds(controls)
            velocity=degree*np.diff(controls,axis=1)
            acceleration=(degree-1)*np.diff(velocity,axis=1)
            a,b=degree-1,degree-2
            cross=np.zeros((count,a+b+1))
            for i in range(a+1):
                for j in range(b+1):
                    weight=comb(a,i)*comb(b,j)/comb(a+b,i+j)
                    cross[:,i+j]+=weight*(velocity[:,i,0]*acceleration[:,j,1]-velocity[:,i,1]*acceleration[:,j,0])
            expected=np.divide(np.abs(cross).max(axis=1),minimum**3,
                               out=np.full(count,np.inf),where=minimum>1e-10)
            np.testing.assert_array_equal(actual,expected)
            assert np.isinf(actual[0])
            cases+=count
    return dict(curves=cases,scalar_product_identical=True,stationary_curve_rejected=True)


def verify_bounds():
    rng = np.random.default_rng(2343)
    controls = np.zeros((256,6,2))
    controls[:,:,0] = np.linspace(0,10,6)
    controls += rng.uniform(-.2,.2,controls.shape)
    low,high,curvature = curve_bounds(controls)
    owners = np.repeat(np.arange(len(controls)),257)
    stations = np.tile(np.linspace(0,1,257),len(controls))
    velocity = evaluate_controls(5*np.diff(controls[owners],axis=1),stations)
    acceleration = evaluate_controls(20*np.diff(controls[owners],n=2,axis=1),stations)
    speed = np.linalg.norm(velocity,axis=1)
    actual = np.abs(velocity[:,0]*acceleration[:,1]-velocity[:,1]*acceleration[:,0])/speed**3
    assert np.all(speed>=low[owners]-1e-12) and np.all(speed<=high[owners]+1e-12)
    assert np.all(actual<=curvature[owners]+1e-12)
    halves = split_controls(controls)
    left = evaluate_controls(halves[::2],np.full(len(controls),.5))
    right = evaluate_controls(halves[1::2],np.full(len(controls),.5))
    assert np.allclose(left,evaluate_controls(controls,np.full(len(controls),.25)),atol=1e-12,rtol=0)
    assert np.allclose(right,evaluate_controls(controls,np.full(len(controls),.75)),atol=1e-12,rtol=0)
    return dict(curves=len(controls),dense_samples=len(owners),speed_and_curvature_bounds=True,subdivision_preserves_curve=True)


def verify_body_acceleration(vehicle):
    rng = np.random.default_rng(2343)
    controls = np.zeros((256,6,2))
    controls[:,:,0] = np.linspace(0,10,6)
    controls += rng.uniform(-.2,.2,controls.shape)
    angles,scale = rng.uniform(-np.pi,np.pi,len(controls)),10**rng.uniform(-1,2,len(controls))
    rotations = np.stack((np.stack((np.cos(angles),-np.sin(angles)),axis=1),
                          np.stack((np.sin(angles),np.cos(angles)),axis=1)),axis=1)
    controls = np.einsum('nij,nkj->nki',rotations,controls)*scale[:,None,None]
    low,high,curvature = curve_bounds(controls)
    radius = np.hypot(max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']),vehicle['widthM']*.5)
    position_bound,body_bound = body_acceleration_bounds(controls,low,high,curvature,radius)
    polynomial = BPoly(controls.transpose(1,0,2)[:,None,:,:],[0,1])
    stations = np.linspace(0,1,257)
    velocity,acceleration,jerk = [polynomial.derivative(order)(stations).transpose(1,0,2) for order in (1,2,3)]
    squared_speed = np.sum(velocity**2,axis=2)
    omega = (velocity[:,:,0]*acceleration[:,:,1]-velocity[:,:,1]*acceleration[:,:,0])/squared_speed
    alpha = (velocity[:,:,0]*jerk[:,:,1]-velocity[:,:,1]*jerk[:,:,0]-
             2*omega*np.sum(velocity*acceleration,axis=2))/squared_speed
    assert np.all(np.linalg.norm(acceleration,axis=2)<=position_bound[:,None]+1e-9)
    assert np.all(np.abs(omega)<=curvature[:,None]*high[:,None]+1e-10)
    heading = np.arctan2(velocity[:,:,1],velocity[:,:,0])
    cosine,sine = np.cos(heading),np.sin(heading)
    corners = [(-vehicle['rearOverhangM'],y) for y in (-vehicle['widthM']*.5,vehicle['widthM']*.5)]
    corners += [(vehicle['frontFromRearAxleM'],y) for y in (-vehicle['widthM']*.5,vehicle['widthM']*.5)]
    for x,y in corners:
        dx,dy = -alpha*y-omega**2*x,alpha*x-omega**2*y
        second = acceleration+np.stack((cosine*dx-sine*dy,sine*dx+cosine*dy),axis=2)
        assert np.all(np.linalg.norm(second,axis=2)<=body_bound[:,None]+1e-8)
    positions = polynomial(stations).transpose(1,0,2)
    outlines = body_outlines(positions.reshape(-1,2),heading.ravel(),vehicle['widthM']*.5,
                            vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']).reshape(len(controls),257,4,2)
    blend = np.linspace(0,1,33)[None,:,None,None]
    for first in range(0,256,32):
        chord = outlines[:,first,None]*(1-blend)+outlines[:,first+32,None]*blend
        deviation = np.linalg.norm(outlines[:,first:first+33]-chord,axis=3).max(axis=(1,2))
        assert np.all(deviation<=body_bound/(8*8**2)+1e-8)
    return dict(curves=len(controls),dense_stations=len(stations),independent_bernstein_derivatives=True,
                rotated_and_scaled_curves=True,continuous_corner_chords_bounded=True)


def verify_long_source(vehicle):
    x = np.linspace(0,350,6)
    points = np.c_[x,12*np.sin(2*np.pi*x/350)]
    entry = dict(position=points[0],heading=np.array([1.,0.]))
    exit = dict(position=points[-1],heading=np.array([1.,0.]))
    controls = spline_controls(points,entry,exit)
    _,speed,curvature = curve_bounds(controls)
    radius = np.hypot(max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']),vehicle['widthM']*.5)
    first_order_poses = int(np.maximum(1,np.ceil(speed*(1+curvature.max()*radius)/.05)).sum()+len(controls))
    assert first_order_poses>4096
    route = shapely.LineString(points)
    path,note = spline_route(entry,exit,route,shapely.buffer(route,5),vehicle,maximum_poses=256)
    assert path is not None and note['vehicle_sweep']['sampled_poses']<=256
    assert np.allclose(shapely.get_coordinates(path)[[0,-1]],points[[0,-1]],atol=1e-7,rtol=0)
    owners = np.repeat(np.arange(len(controls)),33)
    xy = evaluate_controls(controls[owners],np.tile(np.linspace(0,1,33),len(controls)))
    deviation = float(shapely.distance(shapely.points(xy),path).max())
    assert deviation<=note['geometry_deviation_bound_m']+1e-10
    assert note['geometry_deviation_bound_m']<=.025
    return dict(first_order_required_poses=first_order_poses,chosen=note,measured_geometry_deviation_m=deviation)


def verify_route(vehicle):
    theta = np.linspace(0,np.pi,9)
    points = 20*np.c_[np.cos(theta),np.sin(theta)]
    entry = dict(position=points[0],heading=np.array([0.,1.]))
    exit = dict(position=points[-1],heading=np.array([0.,-1.]))
    controls = spline_controls(points,entry,exit)
    velocity = 5*np.diff(controls[[0,-1]],axis=1)
    acceleration = 20*np.diff(controls[[0,-1]],n=2,axis=1)
    derivatives = np.array([velocity[0,0],velocity[1,-1]])
    derivatives /= np.linalg.norm(derivatives,axis=1)[:,None]
    assert np.allclose(derivatives,[entry['heading'],exit['heading']],atol=1e-10,rtol=0)
    assert np.max(np.abs([acceleration[0,0],acceleration[1,-1]]))<1e-10
    route = shapely.LineString(points)
    floor = shapely.buffer(shapely.LineString(np.vstack(([20,-6],points,[-20,-6]))),4)
    path,note = spline_route(entry,exit,route,floor,vehicle)
    assert path is not None and note['vehicle_sweep']['fits_surface']
    assert np.allclose(shapely.get_coordinates(path)[[0,-1]],points[[0,-1]],atol=1e-10,rtol=0)
    gap = shapely.buffer(path.interpolate(.5,normalized=True),.5)
    assert spline_route(entry,exit,route,shapely.difference(floor,gap),vehicle)[0] is None
    assert spline_route(entry,exit,route,floor,vehicle,fairing_weight=.1)[0] is not None
    assert spline_route(entry,exit,route,shapely.difference(floor,gap),vehicle,fairing_weight=.1)[0] is None
    assert spline_route(entry,exit,route,floor,dict(vehicle,minimumRearAxleRadiusM=100))[0] is None
    assert spline_route(entry,exit,route,floor,vehicle,maximum_poses=16)[1]['reason']=='spline_pose_budget_exhausted'
    return dict(endpoint_positions_and_tangents=True,zero_endpoint_curvature=True,body_obstacle_rejected=True,
                radius_limit_enforced=True,pose_budget_enforced=True,chosen=note)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args()
    vehicle = json.loads(Path(__file__).with_name('infrastructure_network_recipes.json').read_text())['vehicleProfiles']['compact']
    report = dict(bounds=verify_bounds(),batched_bounds=verify_batched_bounds(),fairing=verify_fairing(),route=verify_route(vehicle),
                  body_acceleration=verify_body_acceleration(vehicle),long_source=verify_long_source(vehicle))
    paths = [Path(__file__),*(Path(__file__).with_name(name) for name in
             ('infrastructure_spline_routes.py','infrastructure_vehicle_sweep.py','infrastructure_route_fairing.py',
              'infrastructure_network_recipes.json'))]
    report['implementation_sha256'] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))


if __name__=='__main__':
    main()
