"""Fit local C2 road curves with a sparse Jacobian and bounded residual evaluations."""

import numpy as np
import shapely
from scipy.optimize import least_squares
from scipy.sparse import lil_matrix

from infrastructure_spline_routes import evaluate_controls,curve_bounds,split_controls,checked_spline_path
from infrastructure_vehicle_sweep import body_outlines
from infrastructure_junction_surfaces import same_island_winding
from infrastructure_surface_clearance import SurfaceClearance


class ResidualBudgetExceeded(RuntimeError):
    pass


def hermite_controls(state):
    a,b = state[:-1],state[1:]
    return np.stack((a[:,:2],a[:,:2]+a[:,2:4]/5,a[:,:2]+2*a[:,2:4]/5+a[:,4:]/20,
                     b[:,:2]-2*b[:,2:4]/5+b[:,4:]/20,b[:,:2]-b[:,2:4]/5,b[:,:2]),axis=1)


def residual_dependencies(count):
    variables,segments = 6*(count-2)+2,count-1
    pattern = lil_matrix((93*segments+variables,variables),dtype=np.uint8)
    for i in range(segments):
        for node in (i,i+1):
            if 0<node<count-1:
                pattern[i*93:(i+1)*93,(node-1)*6:node*6] = 1
        if i==0:
            pattern[:93,-2] = 1
        if i==segments-1:
            pattern[i*93:(i+1)*93,-1] = 1
    for i in range(variables):
        pattern[93*segments+i,i] = 1
    return pattern.tocsr()


def fit_hermite_route(entry,exit,route,surface,vehicle,error=.025,*,spacing=None,
                      maximum_nodes=18,maximum_evaluations=40,maximum_calls=1024,maximum_poses=4096,clearance=None):
    if min(maximum_nodes-2,maximum_evaluations,maximum_calls)<1 or error<=0 or not np.isfinite(error):
        raise ValueError('node, evaluation and error budgets must be positive')
    spacing = vehicle['minimumRearAxleRadiusM']*2 if spacing is None else spacing
    if spacing<=0 or not np.isfinite(spacing):
        raise ValueError('spacing must be finite and positive')
    if route.length<=1e-7:
        return None,dict(reason='collapsed_hermite_route',residual_calls=0)
    count = max(3,min(maximum_nodes,int(np.ceil(route.length/spacing))+1))
    raw = shapely.get_coordinates(shapely.line_interpolate_point(route,np.linspace(0,route.length,count)))
    raw[[0,-1]] = [entry['position'],exit['position']]
    origin,step = raw[0].copy(),route.length/(count-1)
    raw -= origin
    state = np.zeros((count,6))
    state[:,:2] = raw
    state[1:-1,2:4] = (raw[2:]-raw[:-2])/2
    state[1:-1,4:] = raw[2:]-2*raw[1:-1]+raw[:-2]
    initial = np.r_[state[1:-1].ravel(),step,step]
    owners,stations = np.repeat(np.arange(count-1),17),np.tile(np.linspace(0,1,17),count-1)
    clearance = SurfaceClearance(surface) if clearance is None else clearance
    initial_samples,initial_measured = clearance.samples,clearance.measured_samples
    calls,best_cost,best = 0,np.inf,initial.copy()
    def decode(values):
        result = state.copy()
        result[1:-1] = values[:-2].reshape(-1,6)
        result[0,2:4],result[-1,2:4] = values[-2]*entry['heading'],values[-1]*exit['heading']
        return hermite_controls(result)+origin
    def residual(values):
        nonlocal calls,best_cost,best
        if calls>=maximum_calls:
            raise ResidualBudgetExceeded
        calls += 1
        controls = decode(values)
        xy = evaluate_controls(controls[owners],stations)
        velocity = evaluate_controls(5*np.diff(controls[owners],axis=1),stations)
        refined = controls
        for _ in range(3):
            refined = split_controls(refined)
        _,_,curvature = curve_bounds(refined)
        curvature = np.nan_to_num(curvature,nan=1e6,posinf=1e6)
        corners = body_outlines(xy,np.arctan2(velocity[:,1],velocity[:,0]),vehicle['widthM']*.5,
                               vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
        points = shapely.points(corners.reshape(-1,2))
        floor = 4*clearance.shortfall(points,error*3).reshape(count-1,68)
        bends = 4*np.maximum(curvature.reshape(count-1,8)*vehicle['minimumRearAxleRadiusM']-.96,0)
        moving = np.maximum(.1-np.linalg.norm(velocity,axis=1).reshape(count-1,17)/step,0)
        values_residual = np.r_[np.c_[floor,bends,moving].ravel(),.005*(values-initial)]
        cost = float(np.sum(values_residual**2))
        if cost<best_cost:
            best_cost,best = cost,values.copy()
        return values_residual
    pattern = residual_dependencies(count)
    lower = np.full(len(initial),-np.inf)
    lower[-2:] = step*.1
    status = 'optimizer_finished'
    try:
        fitted = least_squares(residual,initial,jac_sparsity=pattern,bounds=(lower,np.inf),
                               max_nfev=maximum_evaluations,ftol=1e-5,xtol=1e-6,gtol=1e-5)
        values,status = fitted.x,'optimizer_converged' if fitted.success else 'optimizer_evaluation_budget_exhausted'
    except ResidualBudgetExceeded:
        values,status = best,'optimizer_residual_budget_exhausted'
    controls = decode(values)
    for _ in range(3):
        controls = split_controls(controls)
    path,note = checked_spline_path(controls,surface,vehicle,error,maximum_poses)
    if path is not None and not same_island_winding(path,route,surface):
        path,note = None,dict(reason='hermite_changes_source_island_winding')
    note.update(nodes=count,residual_calls=calls,maximum_residual_calls=maximum_calls,optimizer_status=status,
                minimum_objective_seen=best_cost,jacobian_shape=pattern.shape,jacobian_nonzeros=pattern.nnz,
                clearance_samples=clearance.samples-initial_samples,
                clearance_measured_samples=clearance.measured_samples-initial_measured,
                jacobian_pattern_bytes=pattern.data.nbytes+pattern.indices.nbytes+pattern.indptr.nbytes)
    if path is not None:
        note['profile'] = 'C2-quintic-Hermite-fit'
    return path,note


def fitted_route_candidates(entry,exit,route,surface,vehicle,error=.025,maximum_calls=1024,*,clearance=None):
    clearance = SurfaceClearance(surface) if clearance is None else clearance
    initial_samples,initial_measured = clearance.samples,clearance.measured_samples
    calls,trials = 0,[]
    for spacing in (vehicle['minimumRearAxleRadiusM']*2,vehicle['minimumRearAxleRadiusM']):
        if calls>=maximum_calls:
            break
        path,note = fit_hermite_route(entry,exit,route,surface,vehicle,error,spacing=spacing,
                                     maximum_calls=maximum_calls-calls,clearance=clearance)
        calls += note['residual_calls']
        trials.append(dict(spacing_m=spacing,**note))
        if path is not None:
            return path,dict(note,spacing_m=spacing,residual_calls=calls,maximum_residual_calls=maximum_calls,
                             clearance_samples=clearance.samples-initial_samples,
                             clearance_measured_samples=clearance.measured_samples-initial_measured,fit_trials=trials)
    return None,dict(reason='no_clear_fitted_route',residual_calls=calls,maximum_residual_calls=maximum_calls,
                     clearance_samples=clearance.samples-initial_samples,
                     clearance_measured_samples=clearance.measured_samples-initial_measured,fit_trials=trials)
