"""Bounded G1 clothoid fits with analytic endpoint curvature and measured closure error."""

import numpy as np
from functools import cache


@cache
def quadrature(order):
    points,weights = np.polynomial.legendre.leggauss(order)
    return (points+1)*.5,weights*.5


def weighted_sum(values,weights):
    return np.einsum('...j,j->...',values,weights,optimize=False)


def wrapped(angle):
    return (angle+np.pi)%(2*np.pi)-np.pi


def fit_clothoids(start,end,start_heading,end_heading,tolerance=1e-7):
    chord = end-start
    distance = np.linalg.norm(chord,axis=1)
    heading = np.arctan2(chord[:,1],chord[:,0])
    initial = wrapped(start_heading-heading)
    delta = wrapped(end_heading-start_heading)
    a = np.clip(3*(2*initial+delta),-8*np.pi,8*np.pi)
    points,weights = quadrature(16)
    bend = points*(points-1)
    iterations = 0
    for iterations in range(16):
        phase = initial[:,None]+delta[:,None]*points+a[:,None]*bend
        residual = weighted_sum(np.sin(phase),weights)
        if np.max(np.abs(residual),initial=0)<1e-12:
            break
        derivative = weighted_sum(np.cos(phase),weights*bend)
        safe = np.abs(derivative)>1e-12
        step = np.divide(residual,derivative,out=np.zeros_like(residual),where=safe)
        a = np.clip(a-np.clip(step,-np.pi,np.pi),-8*np.pi,8*np.pi)
    integral = weighted_sum(np.exp(1j*(initial[:,None]+delta[:,None]*points+a[:,None]*bend)),weights)
    forward = integral.real>1e-9
    length = np.divide(distance,integral.real,out=np.zeros_like(distance),where=forward)
    check_points,check_weights = quadrature(64)
    check_phase = initial[:,None]+delta[:,None]*check_points+a[:,None]*check_points*(check_points-1)
    check = weighted_sum(np.exp(1j*check_phase),check_weights)
    error = np.abs(length*check-distance)
    valid = forward&(distance>tolerance)&(error<=tolerance)&(length<=distance*2)
    begin_curvature = np.divide(delta-a,length,out=np.zeros_like(length),where=length>0)
    end_curvature = np.divide(delta+a,length,out=np.zeros_like(length),where=length>0)
    return dict(start=start,heading=heading,initial=initial,delta=delta,bend=a,length_m=length,
                begin_curvature_per_m=begin_curvature,end_curvature_per_m=end_curvature,
                maximum_curvature_per_m=np.maximum(np.abs(begin_curvature),np.abs(end_curvature)),
                closure_error_m=error,valid=valid,newton_steps=iterations,
                scope='G1 poses only; road containment, traffic semantics and G2 transitions remain open.')


def sample_clothoids(fit,stations):
    points,weights = quadrature(32)
    t = np.asarray(stations)[:,None]*points
    phase = (fit['initial'][:,None,None]+fit['delta'][:,None,None]*t+
             fit['bend'][:,None,None]*t*(t-1))
    integral = weighted_sum(np.exp(1j*phase),weights)*np.asarray(stations)
    offset = integral*fit['length_m'][:,None]*np.exp(1j*fit['heading'][:,None])
    xy = fit['start'][:,None,:]+np.stack((offset.real,offset.imag),axis=-1)
    heading = (fit['heading'][:,None]+fit['initial'][:,None]+fit['delta'][:,None]*stations+
               fit['bend'][:,None]*stations*(stations-1))
    return xy,heading
