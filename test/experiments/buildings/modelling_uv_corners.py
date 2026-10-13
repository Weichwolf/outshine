"""Validate immutable texture-coordinate sets attached to logical polygon corners."""

from numbers import Integral

from modelling_scope import scalar


def rings_uv(values,rings):
    values=tuple(tuple(tuple(scalar(x) for x in pair) for pair in row) for row in values)
    if len(values)!=len(rings) or any(len(a)!=len(b) for a,b in zip(values,rings)) or any(len(pair)!=2 for row in values for pair in row):
        raise ValueError('Face UV coordinates must match each polygon corner')
    return values


def channels_uv(values,rings):
    result=[]
    for channel,coordinates in values:
        if isinstance(channel,bool) or not isinstance(channel,Integral) or channel<1:
            raise ValueError('Additional logical UV channels need positive integer indices')
        result.append((int(channel),rings_uv(coordinates,rings)))
    if len({channel for channel,_ in result})!=len(result):raise ValueError('Duplicate logical UV channel')
    return tuple(sorted(result))
