"""Fit circle/straight plateaus joined by linear-curvature transitions."""

import numpy as np
from scipy.optimize import least_squares

from transport_hermite_route_fitting import hermite_controls


NODES,WEIGHTS=np.polynomial.legendre.leggauss(16)


def advance(position,heading,start,end,length):
    stations=(NODES+1)*length*.5
    angles=heading+start*stations+(end-start)*stations**2/(2*length)
    delta=np.array([WEIGHTS@np.cos(angles),WEIGHTS@np.sin(angles)])*length*.5
    return position+delta,heading+(start+end)*length*.5


def segments(lengths,radius,sign,transition):
    bends=np.array([0,sign[0],0,sign[1],0] if isinstance(sign,tuple) else [0,sign,-sign,sign,0])/radius
    result=[]
    for i in range(4):
        result.append((bends[i],bends[i+1],transition))
        if i<3:result.append((bends[i+1],bends[i+1],lengths[i]))
    return result


def endpoint(lengths,radius,sign,transition):
    position,heading=np.zeros(2),0.
    for start,end,length in segments(lengths,radius,sign,transition):
        position,heading=advance(position,heading,start,end,length)
    return np.r_[position,heading]


def controls_for(lengths,radius,sign,transition,entry,spacing=1.):
    position=entry['position'].copy();heading=np.arctan2(*entry['heading'][::-1]);pieces=[]
    for start,end,length in segments(lengths,radius,sign,transition):
        count=max(1,int(np.ceil(length/spacing)));step=length/count
        for i in range(count):
            a=start+(end-start)*i/count;b=start+(end-start)*(i+1)/count
            target,angle=advance(position,heading,a,b,step)
            directions=np.array([[np.cos(heading),np.sin(heading)],[np.cos(angle),np.sin(angle)]])
            acceleration=directions[:,::-1]*np.array([-1,1])*np.array([a,b])[:,None]*step**2
            state=np.c_[np.array([position,target]),directions*step,acceleration]
            pieces.append(hermite_controls(state)[0]);position,heading=target,angle
    return np.array(pieces)


def arc_line_lengths(entry,exit,radius,sign,transition):
    left=lambda h:np.array([-h[1],h[0]])
    a=entry['position']+sign[0]*radius*left(entry['heading'])
    b=exit['position']+sign[1]*radius*left(exit['heading'])
    delta=b-a;distance=np.linalg.norm(delta);offset=(sign[1]-sign[0])*radius
    if distance<=1e-8 or abs(offset)>distance:return None
    angle=np.arctan2(delta[1],delta[0])-np.arcsin(offset/distance)
    first=sign[0]*((sign[0]*(angle-np.arctan2(*entry['heading'][::-1])))%(2*np.pi))
    last=sign[1]*((sign[1]*(np.arctan2(*exit['heading'][::-1])-angle))%(2*np.pi))
    return np.maximum([abs(first)*radius-transition,np.sqrt(distance**2-offset**2),
                       abs(last)*radius-transition],1e-5)


def fit_turn(entry,exit,seed,radius,sign,transition,maximum_calls=128):
    if radius<=0 or transition<=0 or maximum_calls<4:raise ValueError('invalid turn recipe budget')
    delta=exit['position']-entry['position'];h=entry['heading'];left=np.array([-h[1],h[0]])
    angles=np.unwrap(np.arctan2(*np.diff(np.array(seed.coords),axis=0)[:,::-1].T))
    target=np.r_[delta@h,delta@left,angles[-1]-angles[0]]
    final=np.arctan2(h[0]*exit['heading'][1]-h[1]*exit['heading'][0],h@exit['heading'])
    target[2]=final+2*np.pi*round((target[2]-final)/(2*np.pi))
    if isinstance(sign,tuple):
        if len(sign)!=2 or any(s not in (-1,1) for s in sign):raise ValueError('invalid arc directions')
        initial=arc_line_lengths(entry,exit,radius,sign,transition)
        if initial is None:return None,dict(reason='seed_has_no_arc_line_tangent',residual_calls=0)
    else:
        changes=np.diff(angles);groups=np.split(changes,np.flatnonzero(np.sign(changes[1:])!=np.sign(changes[:-1]))+1)
        sweeps=[abs(g.sum()) for g in groups if abs(g.sum())>1e-5]
        if len(sweeps)!=3:return None,dict(reason='seed_has_no_three_arc_partition',residual_calls=0)
        initial=np.maximum(np.array(sweeps)*radius-transition,1e-5)
    upper=np.full(3,2*np.pi*radius)
    if isinstance(sign,tuple):upper[1]=max(upper[1],seed.length+4*transition)
    calls=0;best=None;cost=np.inf
    class BudgetExhausted(Exception):pass
    def residual(lengths):
        nonlocal calls,best,cost
        if calls>=maximum_calls:raise BudgetExhausted
        calls+=1;error=endpoint(lengths,radius,sign,transition)-target;error[2]*=radius
        value=error@error
        if value<cost:cost,best=value,lengths.copy()
        return error
    try:
        least_squares(residual,initial,bounds=(1e-6,upper),max_nfev=32,
                      ftol=1e-10,xtol=1e-10,gtol=1e-10)
    except BudgetExhausted:pass
    note=dict(residual_calls=calls,closure_error_m=float(np.sqrt(cost)),radius_m=radius,
              transition_m=transition,reason='endpoint_not_closed')
    if cost>1e-12:return None,note
    return controls_for(best,radius,sign,transition,entry),dict(note,reason='closed',lengths_m=best.tolist())
