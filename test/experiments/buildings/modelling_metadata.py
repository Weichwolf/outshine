"""Immutable authored group names and material values, separate from native palette indices."""

from dataclasses import dataclass,replace
from numbers import Integral,Real
from collections.abc import Mapping

from modelling_scope import scalar


def group_id(value):
    if isinstance(value,bool) or not isinstance(value,Integral) or not 0<=value<=4294967295:
        raise ValueError('A geometry group needs an unsigned 32-bit identifier')
    return int(value)


def value_key(value):
    if isinstance(value,bool):return ('bool',value)
    if isinstance(value,Integral):return ('int',int(value))
    if isinstance(value,Real):return ('float',scalar(value))
    if isinstance(value,str):return ('string',value)
    if isinstance(value,(tuple,list)):return ('array',tuple(value_key(item) for item in value))
    raise ValueError('Material values need finite scalars, strings or arrays')


def immutable_value(value):
    value_key(value)
    if isinstance(value,(tuple,list)):return tuple(immutable_value(item) for item in value)
    return value if isinstance(value,(str,bool)) else int(value) if isinstance(value,Integral) else scalar(value)


@dataclass(frozen=True)
class Material:
    name: str=''
    attributes: tuple=()

    def __post_init__(self):
        if not isinstance(self.name,str):raise ValueError('Material name must be a string')
        rows=self.attributes.items() if isinstance(self.attributes,Mapping) else self.attributes
        rows=tuple((key,immutable_value(value)) for key,value in rows)
        if any(not isinstance(key,str) or not key for key,_ in rows) or len({key for key,_ in rows})!=len(rows):
            raise ValueError('Material attributes need unique nonempty string keys')
        object.__setattr__(self,'attributes',tuple(sorted(rows)))

    def key(self):return self.name,tuple((key,value_key(value)) for key,value in self.attributes)


def named_groups(values):
    rows=values.items() if isinstance(values,Mapping) else values
    rows=tuple((group_id(key),value) for key,value in rows)
    if any(not isinstance(value,str) for _,value in rows) or len({key for key,_ in rows})!=len(rows):
        raise ValueError('Group names need unique identifiers and string values')
    return tuple(sorted(rows))


def materials(values):
    rows=values.items() if isinstance(values,Mapping) else values
    result=[]
    for key,value in rows:
        identifier=group_id(key)
        if identifier>65535 or not isinstance(value,Material):raise ValueError('Material definitions need 16-bit identifiers and Material records')
        result.append((identifier,value))
    if len({key for key,_ in result})!=len(result):raise ValueError('Material definition identifiers must be unique')
    return tuple(sorted(result))


def inherit(result,source):
    return replace(result,geometry=replace(result.geometry,group_names=source.geometry.group_names,material_defs=source.geometry.material_defs))


def validate_membership(fields,polygons,edges,points,triangle_count):
    import numpy as np
    if polygons:
        ids=fields.get('parts')
        if ids is None or ids.shape!=(triangle_count,) or ids.dtype.kind not in 'iu' or (len(ids) and (ids.min()<0 or ids.max()>=len(polygons))):
            raise ValueError('Logical faces require valid triangle-to-face indices')
        for name,attribute,default in [('materials','material',1),('groups','group',0)]:
            expected=np.asarray([getattr(face,attribute) for face in polygons])[ids]
            actual=fields.get(name,np.full(triangle_count,default))
            if not np.array_equal(expected,actual):raise ValueError('Logical face '+attribute+' differs from native triangles')
    for name,elements in [('edges',edges),('points',points)]:
        if not elements:continue
        if name not in fields or not np.array_equal(fields[name],[element.vertices for element in elements]):
            raise ValueError('Logical '+name+' differ from native indices')
        for suffix,attribute,default in [('_materials','material',1),('_groups','group',0)]:
            actual=fields.get(name+suffix,np.full(len(elements),default))
            if not np.array_equal(actual,[getattr(element,attribute) for element in elements]):
                raise ValueError('Logical '+name+' '+attribute+' differs from native data')
