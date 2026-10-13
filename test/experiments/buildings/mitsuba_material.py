"""Resolve core CGA material values into a labelled physical reference BSDF."""

import numpy as np

from modelling_material import canonical
from modelling_material_schema import DEFAULTS,srgb_to_linear


def reference(material):
    values=DEFAULTS|canonical(material)
    if any(values[key] for key in ('colormap','normalmap','roughnessmap','metallicmap','opacitymap')):
        raise NotImplementedError('CGA reference texture resource binding remains pending')
    rgb=srgb_to_linear([values['color.'+axis] for axis in ('r','g','b')])
    if np.any(rgb<0) or np.any(rgb>1):raise ValueError('Physical reference requires valid colour reflectance')
    roughness=float(values['roughness']);metallic=float(values['metallic']);opacity=float(values['opacity'])
    if not all(0<=value<=1 for value in (roughness,metallic,opacity)):raise ValueError('Physical reference material factors must lie in [0,1]')
    bsdf=dict(type='principled',base_color=dict(type='rgb',value=rgb.tolist()),roughness=roughness,metallic=metallic)
    if values['doubleSided']:bsdf=dict(type='twosided',nested=bsdf)
    if opacity<1:bsdf=dict(type='blendbsdf',weight=1.-opacity,bsdf_0=bsdf,bsdf_1=dict(type='null'))
    emission=srgb_to_linear([values['emissive.'+axis] for axis in ('r','g','b')])
    emitter=dict(type='area',radiance=dict(type='rgb',value=emission.tolist())) if np.any(emission>0) else None
    return bsdf,emitter,dict(shader='Mitsuba principled approximation of core CGA factors; legacy Phong equivalence unproved',
        colour_space='CGA sRGB converted to linear RGB exactly once',linear_base_colour=rgb.tolist(),roughness=roughness,metallic=metallic,opacity=opacity)
