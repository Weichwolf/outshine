"""Symmetric clothoid-circle-clothoid turns with zero endpoint curvature."""

import numpy as np

from infrastructure_clothoids import quadrature,weighted_sum,wrapped


def unit_displacement(delta,transition,order):
    points,weights = quadrature(order)
    phase = delta[:,None]*transition[:,None]*points**2/(2*(1-transition[:,None]))
    circular = 1-2*transition
    middle = circular*np.exp(.5j*delta)*np.sinc(delta*circular/(2*(1-transition)*np.pi))
    return transition*weighted_sum(np.exp(1j*phase)+np.exp(1j*(delta[:,None]-phase)),weights)+middle


def fit_symmetric_turns(node,trim,start_heading,end_heading,tolerance=1e-7,transition_fraction=.5):
    transition = np.broadcast_to(np.asarray(transition_fraction,dtype=float),trim.shape)
    if not np.all(np.isfinite(transition)) or np.any((transition<=0)|(transition>.5)):
        raise ValueError('clothoid transition fraction must lie in (0,.5]')
    delta = wrapped(end_heading-start_heading)
    incoming = np.stack((np.cos(start_heading),np.sin(start_heading)),axis=-1)
    outgoing = np.stack((np.cos(end_heading),np.sin(end_heading)),axis=-1)
    start,end = node-trim[:,None]*incoming,node+trim[:,None]*outgoing
    integral = unit_displacement(delta,transition,16)
    forward = (integral*np.exp(-.5j*delta)).real
    chord = np.linalg.norm(end-start,axis=-1)
    length = np.divide(chord,forward,out=np.zeros_like(chord),where=forward>1e-9)
    check = unit_displacement(delta,transition,64)
    error = np.abs(length*check*np.exp(1j*start_heading)-
                   ((end[:,0]-start[:,0])+1j*(end[:,1]-start[:,1])))
    maximum = np.divide(np.abs(delta),length*(1-transition),out=np.zeros_like(length),where=length>0)
    return dict(start=start,end=end,start_heading=start_heading,delta=delta,length_m=length,
                maximum_curvature_per_m=maximum,closure_error_m=error,transition_fraction=transition,
                maximum_curvature_rate_per_m2=np.divide(maximum,transition*length,
                    out=np.zeros_like(length),where=length>0),
                valid=(chord>tolerance)&(forward>1e-9)&(error<=tolerance),
                scope='G2 joins between straight approaches; asymmetric and curved approaches remain open.')


def sample_symmetric_turns(fit,stations):
    stations = np.asarray(stations)
    points,weights = quadrature(16)
    transition = fit['transition_fraction'][:,None]
    first = np.minimum(stations,transition)
    second = np.clip(stations-transition,0,1-2*transition)
    third = np.maximum(stations-(1-transition),0)
    a,c = first[:,:,None]*points,third[:,:,None]*points
    delta = fit['delta'][:,None,None]
    denominator = 2*transition[:,:,None]*(1-transition[:,:,None])
    integral = (first*weighted_sum(np.exp(1j*delta*a*a/denominator),weights)+
                second*np.exp(1j*fit['delta'][:,None]*(transition+second)/(2*(1-transition)))*
                    np.sinc(fit['delta'][:,None]*second/(2*(1-transition)*np.pi))+
                third*weighted_sum(np.exp(1j*delta*(1-(transition[:,:,None]-c)**2/denominator)),weights))
    offset = integral*fit['length_m'][:,None]*np.exp(1j*fit['start_heading'][:,None])
    xy = fit['start'][:,None,:]+np.stack((offset.real,offset.imag),axis=-1)
    progress = np.where(stations<=transition,stations**2/(2*transition*(1-transition)),
               np.where(stations<1-transition,(stations-transition*.5)/(1-transition),
                        1-(1-stations)**2/(2*transition*(1-transition))))
    heading = fit['start_heading'][:,None]+fit['delta'][:,None]*progress
    curvature = (fit['delta'][:,None]/(fit['length_m'][:,None]*(1-transition))*
                 np.minimum(np.minimum(stations/transition,(1-stations)/transition),1))
    return xy,heading,curvature
