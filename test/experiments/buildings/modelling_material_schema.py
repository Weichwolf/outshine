"""Documented CGA material defaults and typed values, with sRGB colours kept explicit."""

from collections.abc import Mapping
import re

import numpy as np

from outshine_ref.buildings.modelling.scope import scalar


MAPS=('colormap','bumpmap','dirtmap','specularmap','opacitymap','normalmap','emissivemap','occlusionmap','roughnessmap','metallicmap')
DEFAULTS=dict(name='',shader='CityEngineShader',opacity=1.,reflectivity=0.,shininess=1.,bumpValue=1.,metallic=0.,roughness=1.,doubleSided=True)
DEFAULTS.update({family+'.'+axis:1. if family=='color' else 0. for family in ('color','ambient','specular','emissive') for axis in ('r','g','b')})
DEFAULTS.update({key:'' for key in MAPS})
DEFAULTS.update({key+'.'+suffix:1. if suffix in ('su','sv') else 0. for key in MAPS for suffix in ('su','sv','tu','tv','rw')})
DEFAULTS.update({'opacitymap.mode':'blend','opacitymap.cutoff':.5})


def colour(values):
    if len(values)==1 and isinstance(values[0],str):
        text=values[0]
        if not re.fullmatch(r'#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?',text):raise ValueError('CGA colour needs #rrggbb or #rrggbbaa')
        result=[int(text[i:i+2],16)/255. for i in range(1,len(text),2)]
        return tuple(result)
    if len(values) not in (3,4):raise ValueError('CGA colour requires RGB or RGBA')
    return tuple(float(np.clip(scalar(value),0.,1.)) for value in values)


def typed(key,value):
    if key not in DEFAULTS:raise ValueError('Unknown CGA material attribute: '+key)
    default=DEFAULTS[key]
    if isinstance(default,bool):
        if isinstance(value,str) and value in ('true','false'):return value=='true'
        if not isinstance(value,bool):raise ValueError('Material boolean expected: '+key)
        return value
    if isinstance(default,str):
        if not isinstance(value,str):raise ValueError('Material string expected: '+key)
        if key=='opacitymap.mode' and value not in ('blend','mask','opaque'):raise ValueError('Unknown opacity map mode')
        return value
    return scalar(float(value)) if isinstance(value,str) else scalar(value)


def parameters(values):
    if isinstance(values,Mapping):rows=list(values.items())
    else:
        values=list(values)
        if values and all(isinstance(row,(tuple,list)) and len(row)==2 for row in values):rows=values
        else:
            if len(values)%2:raise ValueError('Material array needs complete key/value pairs')
            rows=list(zip(values[::2],values[1::2],strict=True))
    result={}
    for key,value in rows:
        if not isinstance(key,str) or key.startswith('material.'):raise ValueError('Material attribute names omit material. prefix')
        if key.endswith('.rgb') and key.split('.')[0] in ('color','ambient','specular','emissive'):
            rgb=colour((value,))
            if len(rgb)!=3:raise ValueError('Material RGB alias does not accept opacity')
            result.update({key[:-3]+axis:component for axis,component in zip(('r','g','b'),rgb,strict=True)})
        else:result[key]=typed(key,value)
    return result


def srgb_to_linear(value):
    value=np.asarray(value,dtype=float)
    return np.where(value<=.04045,value/12.92,((value+.055)/1.055)**2.4)


def linear_to_srgb(value):
    value=np.asarray(value,dtype=float)
    return np.where(value<=.0031308,12.92*value,1.055*np.maximum(value,0.)**(1./2.4)-.055)
