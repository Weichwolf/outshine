"""Separate rule material overrides from immutable geometry material definitions."""

from dataclasses import replace

import numpy as np

from modelling_metadata import Material
from modelling_material_schema import DEFAULTS,colour,parameters,linear_to_srgb


def canonical(material):
    raw=dict(material.attributes);result={key:value for key,value in raw.items() if key in DEFAULTS}
    if material.name:result.setdefault('name',material.name)
    pbr=raw.get('pbrMetallicRoughness.baseColorFactor')
    if pbr is not None:
        rgb=linear_to_srgb(pbr[:3])
        for axis,value in zip(('r','g','b'),rgb,strict=True):result.setdefault('color.'+axis,float(value))
        result.setdefault('opacity',pbr[3])
    for source,target in [('pbrMetallicRoughness.metallicFactor','metallic'),('pbrMetallicRoughness.roughnessFactor','roughness'),('doubleSided','doubleSided')]:
        if source in raw:result.setdefault(target,raw[source])
    if 'emissiveFactor' in raw:
        for axis,value in zip(('r','g','b'),linear_to_srgb(raw['emissiveFactor']),strict=True):result.setdefault('emissive.'+axis,float(value))
    if 'kd' in raw:
        for axis,value in zip(('r','g','b'),raw['kd'],strict=True):result.setdefault('color.'+axis,float(value))
        if 'd' in raw:result.setdefault('opacity',float(raw['d']))
    return result


def geometry_ids(shape):
    geometry=shape.geometry;result=[]
    for key,elements in [('materials',geometry.polygons),('edges_materials',geometry.edges),('points_materials',geometry.points)]:
        values=geometry.fields.get(key)
        if values is not None:result.extend(int(value) for value in values)
        else:result.extend(element.material for element in elements)
    return tuple(dict.fromkeys(result)) or (1,)


def resolved_materials(shape):
    definitions=dict(shape.geometry.material_defs);override=dict(shape.material.attributes)
    return {identifier:Material('',dict(definitions.get(identifier,Material()).attributes)|canonical(definitions.get(identifier,Material()))|override)
            for identifier in geometry_ids(shape)}


def get_material(shape,source='used',mode='changed'):
    if source not in ('shape','geometry','used') or mode not in ('changed','all'):raise ValueError('Unknown material selector or mode')
    identifier=geometry_ids(shape)[0];definition=dict(shape.geometry.material_defs).get(identifier,Material())
    values={} if source=='shape' else canonical(definition)
    if source!='geometry':values.update(dict(shape.material.attributes))
    if mode=='all':values=DEFAULTS|values
    return tuple((key,'true' if value is True else 'false' if value is False else format(value,'.17g') if isinstance(value,(float,int)) else value)
                 for key,value in sorted(values.items()))


def set_material(shape,values):
    overrides=dict(shape.material.attributes)|parameters(values)
    return replace(shape,material=Material('',overrides))


def color(shape,*values):
    values=colour(values);overrides={'color.'+axis:value for axis,value in zip(('r','g','b'),values[:3],strict=True)}
    if len(values)==4:overrides['opacity']=values[3]
    return set_material(shape,overrides)


def reset_material(shape,target='shape'):
    if target not in ('shape','geometry','all'):raise ValueError('Unknown material reset target')
    if target in ('geometry','all'):
        definitions=tuple((identifier,Material()) for identifier in geometry_ids(shape))
        shape=replace(shape,geometry=replace(shape.geometry,material_defs=definitions))
    return replace(shape,material=Material()) if target in ('shape','all') else shape
