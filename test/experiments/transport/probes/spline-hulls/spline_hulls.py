import numpy as np,shapely
import transport_spline_routes as source
from transport_vehicle_sweep import swept_hull_check


def acceleration_bounds(controls,minimum,speed,curvature,radius):
    degree=controls.shape[1]-1
    acceleration=degree*(degree-1)*np.diff(controls,n=2,axis=1)
    jerk=degree*(degree-1)*(degree-2)*np.diff(controls,n=3,axis=1)
    maximum_acceleration=np.linalg.norm(acceleration,axis=2).max(axis=1)
    maximum_jerk=np.linalg.norm(jerk,axis=2).max(axis=1)
    omega=curvature*minimum
    alpha=(speed*maximum_jerk+2*omega*speed*maximum_acceleration)/minimum**2
    return maximum_acceleration,maximum_acceleration+radius*(alpha+omega**2)


def spline_route(entry,exit,route,surface,vehicle,error=.025,maximum_poses=4096,spacing=None,fairing_weight=0.):
    if spacing is None:raw=shapely.get_coordinates(route)
    else:
        count=max(2,int(np.ceil(route.length/spacing))+1)
        if count>66:return None,dict(reason='spline_knot_budget_exhausted')
        raw=shapely.get_coordinates(shapely.line_interpolate_point(route,np.linspace(0,route.length,count)))
    if len(raw)>66:return None,dict(reason='spline_knot_budget_exhausted')
    points=raw.copy();points[[0,-1]]=[entry['position'],exit['position']]
    controls=source.spline_controls(source.fair_route(points,fairing_weight),entry,exit)
    if controls is None:return None,dict(reason='collapsed_spline_interval')
    minimum,speed,curvature=source.curve_bounds(controls);bound=float(curvature.max())
    if not np.isfinite(bound) or bound*vehicle['minimumRearAxleRadiusM']>1+1e-12:
        return None,dict(reason='spline_curvature_bound_exceeds_vehicle',maximum_curvature_bound=bound if np.isfinite(bound) else None)
    radius=np.hypot(max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']),vehicle['widthM']*.5)
    position_acceleration,body_acceleration=acceleration_bounds(controls,minimum,speed,curvature,radius)
    velocities=5*np.diff(controls,axis=1)
    def evaluate(owners,stations):
        xy=source.evaluate_controls(controls[owners],stations)
        velocity=source.evaluate_controls(velocities[owners],stations)
        return xy,np.arctan2(velocity[:,1],velocity[:,0])
    body=swept_hull_check(surface,evaluate,body_acceleration,vehicle,error,maximum_poses)
    if not body['fits_surface']:
        return None,dict(reason='spline_pose_budget_exhausted' if body.get('reason')=='vehicle_sweep_budget_exhausted' else 'spline_vehicle_body_outside_surface')
    counts=np.maximum(1,np.ceil(np.sqrt(position_acceleration/(8*error))).astype(int))
    owners=np.repeat(np.arange(len(controls)),counts+1)
    stations=np.concatenate([np.linspace(0,1,n+1) for n in counts])
    xy=source.evaluate_controls(controls[owners],stations)
    xy=xy[np.r_[True,np.linalg.norm(np.diff(xy,axis=0),axis=1)>1e-10]]
    path=shapely.LineString(xy)
    if not shapely.is_simple(path):return None,dict(reason='self_intersecting_spline_route')
    return path,dict(profile='G2-quintic-spline',length_m=path.length,vehicle_sweep=body,
        minimum_radius_m=1/bound if bound>0 else None,curvature_bound_per_m=bound,endpoint_curvature_per_m=0.,
        geometry_deviation_bound_m=float((position_acceleration/counts**2/8).max()),geometry_vertices=len(xy))
