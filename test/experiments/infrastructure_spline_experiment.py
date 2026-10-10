"""Verify Bernstein bounds and clamped source splines before real-network use."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely

from infrastructure_spline_routes import curve_bounds,evaluate_controls,split_controls,spline_controls,spline_route
from infrastructure_route_fairing import fair_route


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
    report = dict(bounds=verify_bounds(),fairing=verify_fairing(),route=verify_route(vehicle))
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))


if __name__=='__main__':
    main()
