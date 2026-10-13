"""Resolve core CGA material values into a labelled physical reference BSDF."""

import numpy as np
from io import BytesIO
from PIL import Image

from modelling_material import canonical
from modelling_material_schema import DEFAULTS,srgb_to_linear


def bitmap(path,textures,kind,factor=1.):
    import mitsuba as mi
    matches=[texture for texture in textures if texture.path==path]
    if len(matches)!=1:raise NotImplementedError('Texture resource is missing or ambiguous: '+path)
    if kind=='opacity' and not matches[0].has_alpha:
        raise NotImplementedError('RGB-only opacity interpretation requires a separate reference control')
    with Image.open(BytesIO(matches[0].encoded)) as image:
        if kind=='opacity':
            pixels=np.asarray(image.convert('RGBA'),dtype=np.float32)[:,:,3]/255.
        elif kind=='color':
            pixels=srgb_to_linear(np.asarray(image.convert('RGB'),dtype=np.float32)/255.)
        elif kind=='normal':
            pixels=np.asarray(image.convert('RGB'),dtype=np.float32)/255.
        else:
            pixels=np.asarray(image.convert('RGB'),dtype=np.float32)[:,:,0]/255.
    pixels=np.ascontiguousarray(pixels*factor,dtype=np.float32)
    return dict(type='bitmap',bitmap=mi.Bitmap(pixels),raw=True,filter_type='bilinear',wrap_mode='repeat')


def reference(material,textures=()):
    values=DEFAULTS|canonical(material)
    for key in ('bumpmap','dirtmap','specularmap','emissivemap','occlusionmap'):
        if values[key]:raise NotImplementedError('Physical reference map is not bound: '+key)
    for key in ('colormap','normalmap','roughnessmap','metallicmap','opacitymap'):
        if values[key] and not textures:raise NotImplementedError('CGA reference texture resource binding requires prepared bytes')
        if values[key] and any(values[key+'.'+suffix]!=DEFAULTS[key+'.'+suffix] for suffix in ('su','sv','tu','tv','rw')):
            raise NotImplementedError('Per-map transformations require an explicit projection binding')
    if values['opacitymap'] and values['opacitymap.mode']!='blend':
        raise NotImplementedError('Alpha mask and opaque map modes require separate reference controls')
    rgb=srgb_to_linear([values['color.'+axis] for axis in ('r','g','b')])
    if np.any(rgb<0) or np.any(rgb>1):raise ValueError('Physical reference requires valid colour reflectance')
    roughness=float(values['roughness']);metallic=float(values['metallic']);opacity=float(values['opacity'])
    if not all(0<=value<=1 for value in (roughness,metallic,opacity)):raise ValueError('Physical reference material factors must lie in [0,1]')
    bsdf=dict(type='principled',base_color=dict(type='rgb',value=rgb.tolist()),roughness=roughness,metallic=metallic)
    if values['colormap']:bsdf['base_color']=bitmap(values['colormap'],textures,'color',rgb)
    for key in ('roughness','metallic'):
        if values[key+'map']:bsdf[key]=bitmap(values[key+'map'],textures,key,values[key])
    if values['normalmap']:bsdf=dict(type='normalmap',normalmap=bitmap(values['normalmap'],textures,'normal'),nested=bsdf)
    if values['doubleSided']:bsdf=dict(type='twosided',nested=bsdf)
    if values['opacitymap']:bsdf=dict(type='mask',opacity=bitmap(values['opacitymap'],textures,'opacity',opacity),nested=bsdf)
    elif opacity<1:bsdf=dict(type='blendbsdf',weight=1.-opacity,bsdf_0=bsdf,bsdf_1=dict(type='null'))
    emission=srgb_to_linear([values['emissive.'+axis] for axis in ('r','g','b')])
    emitter=dict(type='area',radiance=dict(type='rgb',value=emission.tolist())) if np.any(emission>0) else None
    return bsdf,emitter,dict(shader='Mitsuba principled approximation of core CGA factors; legacy Phong equivalence unproved',
        colour_space='CGA sRGB converted to linear RGB exactly once',linear_base_colour=rgb.tolist(),roughness=roughness,metallic=metallic,opacity=opacity)
