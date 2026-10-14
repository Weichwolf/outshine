"""Coded procedural radial prototypes; source type remains the classifier's interpretation."""

import math

import numpy as np
import shapely

from outshine_ref.buildings.modelling.radial_profile import RadialProfile


SUPPORTED={'chimney','mast','antenna','tower','tower_section','cooling_tower','storage_tank','silo','industrial_round_body'}


def profile(identifier,polygon,decision,materials=(1,1,9)):
    if decision.kind not in SUPPORTED:
        return None
    centre=polygon.centroid
    if not polygon.contains(centre):
        return None
    radius=polygon.boundary.distance(centre)
    if radius<=0:
        return None
    m=decision.metrics;kind=decision.kind;inner=None
    if kind=='cooling_tower':
        u=np.linspace(0,1,33);neck=.72;ratio=.55
        curvature=math.sqrt(1/ratio**2-1)/neck
        outer=radius*ratio*np.sqrt(1+((u-neck)*curvature)**2)
        thickness=min(.35,radius*.03);inner=outer-thickness
        form='procedural hyperboloid shell, inferred proportions; no tower blueprint'
    elif kind=='chimney':
        u=np.array([0.,.85,1.]);outer=radius*np.array([1.,.76,.72])
        thickness=min(.7,radius*.12);inner=outer-thickness
        form='procedural tapered hollow flue; supplied vertical interval'
    elif kind in ('mast','antenna'):
        u=np.array([0.,.6,.94,1.]);outer=radius*np.array([1.,.65,.18,.04])
        form='procedural tapered shaft; antenna function is unproven'
    elif kind=='industrial_round_body':
        u=np.r_[0.,.5,np.linspace(.55,1.,17)];t=(u-.5)/.5
        outer=np.where(u<=.5,radius,radius*np.sqrt(np.maximum(0.,1-t*t)))
        form='procedural domed industrial body; process and function unproven'
    elif kind=='tower_section':
        u=np.array([0.,.2,.8,1.]);outer=radius*np.array([.9,1.,1.,.9])
        form='procedural rounded elevated section; supplied lower/upper limits'
    elif kind in ('storage_tank','silo'):
        u=np.array([0.,.85,1.]);outer=radius*np.array([1.,1.,.35])
        form='procedural storage-scale cap; actual storage use unproven'
    else:
        u=np.array([0.,.8,1.]);outer=radius*np.array([1.,.8,.75])
        form='procedural tapered tower shaft; exact function and shape unproven'
    inner=np.zeros(len(u)) if inner is None else inner
    heights=m.bottom_m+u*m.height_m;heights[0]=m.bottom_m;heights[-1]=m.top_m
    knots=tuple(zip(heights,outer,inner,strict=True))
    result=RadialProfile(identifier,tuple(centre.coords[0]),knots,materials)
    return result,dict(form=form,source_kind=kind,classification_origin=decision.origin,
        alternatives=decision.alternatives,dimensions='source heights; radial/profile proportions procedural',
        placement='centred inside adjusted footprint; base disk inscribed, source polygon retained',
        source_footprint_area_m2=polygon.area,base_disk_area_m2=math.pi*radius*radius,
        functional_status='structure prototype; doors, ladders, service platforms and interior access incomplete')
