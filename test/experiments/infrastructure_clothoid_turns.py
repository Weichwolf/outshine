"""Symmetric two-clothoid turns with zero curvature at straight approaches."""

import numpy as np

from infrastructure_clothoids import quadrature,weighted_sum,wrapped


def fit_symmetric_turns(node,trim,start_heading,end_heading,tolerance=1e-7):
    delta = wrapped(end_heading-start_heading)
    incoming = np.stack((np.cos(start_heading),np.sin(start_heading)),axis=-1)
    outgoing = np.stack((np.cos(end_heading),np.sin(end_heading)),axis=-1)
    start,end = node-trim[:,None]*incoming,node+trim[:,None]*outgoing
    points,weights = quadrature(16)
    phase = .5*delta[:,None]*points**2
    integral = .5*weighted_sum(np.exp(1j*phase)+np.exp(1j*(delta[:,None]-phase)),weights)
    forward = (integral*np.exp(-.5j*delta)).real
    chord = np.linalg.norm(end-start,axis=-1)
    length = np.divide(chord,forward,out=np.zeros_like(chord),where=forward>1e-9)
    check_points,check_weights = quadrature(64)
    check_phase = .5*delta[:,None]*check_points**2
    check = .5*weighted_sum(np.exp(1j*check_phase)+np.exp(1j*(delta[:,None]-check_phase)),check_weights)
    error = np.abs(length*check*np.exp(1j*start_heading)-
                   ((end[:,0]-start[:,0])+1j*(end[:,1]-start[:,1])))
    maximum = np.divide(2*np.abs(delta),length,out=np.zeros_like(length),where=length>0)
    return dict(start=start,end=end,start_heading=start_heading,delta=delta,length_m=length,
                maximum_curvature_per_m=maximum,closure_error_m=error,
                valid=(chord>tolerance)&(forward>1e-9)&(error<=tolerance),
                scope='G2 joins between straight approaches; asymmetric and curved approaches remain open.')


def sample_symmetric_turns(fit,stations):
    stations = np.asarray(stations)
    points,weights = quadrature(16)
    first,second = np.minimum(stations,.5),np.maximum(stations-.5,0)
    a,b = first[:,None]*points,second[:,None]*points
    delta = fit['delta'][:,None,None]
    integral = (first*weighted_sum(np.exp(2j*delta*a*a),weights)+
                second*weighted_sum(np.exp(1j*delta*(.5+2*b-2*b*b)),weights))
    offset = integral*fit['length_m'][:,None]*np.exp(1j*fit['start_heading'][:,None])
    xy = fit['start'][:,None,:]+np.stack((offset.real,offset.imag),axis=-1)
    progress = np.where(stations<=.5,2*stations**2,1-2*(1-stations)**2)
    heading = fit['start_heading'][:,None]+fit['delta'][:,None]*progress
    curvature = (4*fit['delta'][:,None]/fit['length_m'][:,None]*
                 np.minimum(stations,1-stations))
    return xy,heading,curvature
