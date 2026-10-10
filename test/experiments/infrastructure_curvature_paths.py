"""Bounded G2 pose interpolation with polynomial heading and analytic curvature extrema."""

import numpy as np

from infrastructure_clothoids import quadrature,weighted_sum,wrapped


def heading_progress(stations,delta,amplitude):
    return delta*(3*stations**2-2*stations**3)+amplitude*16*stations**2*(1-stations)**2


def curvature_extrema(delta,amplitude,length):
    a,b,c = 192*amplitude,-12*delta-192*amplitude,6*delta+32*amplitude
    discriminant = np.sqrt(144*delta**2+12288*amplitude**2)
    q = -.5*(b+np.copysign(discriminant,b))
    first = np.divide(q,a,out=np.zeros_like(a),where=a!=0)
    second = np.divide(c,q,out=np.zeros_like(c),where=q!=0)
    stations = np.clip(np.stack((first,second),axis=1),0,1)
    gradient = stations*(1-stations)*(6*delta[:,None]+32*amplitude[:,None]*(1-2*stations))
    return np.divide(np.abs(gradient).max(axis=1),length,out=np.zeros_like(length),where=length>0)


def fit_curvature_paths(start,end,start_heading,end_heading,tolerance=1e-7):
    start,end = np.asarray(start,dtype=float),np.asarray(end,dtype=float)
    delta = wrapped(end_heading-start_heading)
    displacement = end-start
    chord = np.linalg.norm(displacement,axis=1)
    direction = np.arctan2(displacement[:,1],displacement[:,0])
    relative = wrapped(start_heading-direction)
    points,weights = quadrature(32)
    transition,bump = 3*points**2-2*points**3,16*points**2*(1-points)**2
    amplitude = np.clip(-wrapped(relative+delta*.5)/(8/15),-np.pi,np.pi)
    for _ in range(16):
        phase = relative[:,None]+delta[:,None]*transition+amplitude[:,None]*bump
        residual = weighted_sum(np.sin(phase),weights)
        derivative = weighted_sum(bump*np.cos(phase),weights)
        step = np.divide(residual,derivative,out=np.zeros_like(residual),where=np.abs(derivative)>1e-9)
        amplitude = np.clip(amplitude-np.clip(step,-np.pi*.5,np.pi*.5),-np.pi,np.pi)
        if np.all(np.abs(residual)<1e-12):
            break
    phase = relative[:,None]+delta[:,None]*transition+amplitude[:,None]*bump
    forward = weighted_sum(np.cos(phase),weights)
    length = np.divide(chord,forward,out=np.zeros_like(chord),where=forward>1e-9)
    points,weights = quadrature(64)
    check = weighted_sum(np.exp(1j*(start_heading[:,None]+
        heading_progress(points,delta[:,None],amplitude[:,None]))),weights)*length
    closure = np.abs(check-(displacement[:,0]+1j*displacement[:,1]))
    return dict(start=start,end=end,start_heading=start_heading,delta=delta,amplitude=amplitude,
                length_m=length,closure_error_m=closure,
                maximum_curvature_per_m=curvature_extrema(delta,amplitude,length),
                valid=(chord>tolerance)&(forward>1e-9)&(length<=chord*2)&(closure<=tolerance))


def sample_curvature_paths(fit,stations):
    stations = np.asarray(stations)
    points,weights = quadrature(32)
    local = stations[:,None]*points
    phase = fit['start_heading'][:,None,None]+heading_progress(local,
        fit['delta'][:,None,None],fit['amplitude'][:,None,None])
    integral = weighted_sum(np.exp(1j*phase),weights)*stations*fit['length_m'][:,None]
    xy = fit['start'][:,None,:]+np.stack((integral.real,integral.imag),axis=-1)
    heading = fit['start_heading'][:,None]+heading_progress(stations,
        fit['delta'][:,None],fit['amplitude'][:,None])
    gradient = stations*(1-stations)*(6*fit['delta'][:,None]+32*fit['amplitude'][:,None]*(1-2*stations))
    curvature = gradient/fit['length_m'][:,None]
    return xy,heading,curvature
