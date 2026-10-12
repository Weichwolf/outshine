"""Direct Python predicates for CGA face directions, inclinations and hierarchical tags."""

from dataclasses import dataclass
from numbers import Real
import re

import numpy as np

from modelling_scope import scalar


HORIZONTAL=0.9807852804032304
VERTICAL=0.19509032201612833
NUTANT_HIGH=-0.1950903220161282
DIRECTION_EPSILON=1e-14
AXES={'front':(2,1),'back':(2,-1),'left':(0,-1),'right':(0,1),'top':(1,1),'bottom':(1,-1)}
WORLD={'south':'front','north':'back','west':'left','east':'right','up':'top','down':'bottom','side':'side'}


@dataclass(frozen=True)
class Selector:
    name: str
    function: object

    def __call__(self,query):
        result=self.function(query)
        if not isinstance(result,(bool,np.bool_)):raise ValueError('A component selector must return a boolean')
        return bool(result)

    def __and__(self,other):
        other=select(other)
        return Selector('('+self.name+' && '+other.name+')',lambda query:self(query) and other(query))

    def __or__(self,other):
        other=select(other)
        return Selector('('+self.name+' || '+other.name+')',lambda query:self(query) or other(query))

    def __invert__(self):return Selector('!('+self.name+')',lambda query:not self(query))

    def __bool__(self):raise TypeError('Combine component selectors with &, | and ~')


def direction(normal,name):
    axis,sign=AXES[name];value=normal[axis]*sign
    return value>0. and all(abs(other)<=value+DIRECTION_EPSILON for other in normal)


def oriented(query,name):
    if name.startswith('world.'):
        name=WORLD[name.removeprefix('world.')];normal=query.normal
    elif name.startswith(('object.','street.')):
        name=name.partition('.')[2];normal=query.object_normal
    else:normal=tuple(value*(-1. if size<0 else 1.) for value,size in zip(query.scope_normal,query.scope_size,strict=True))
    if name in AXES:return direction(normal,name)
    if name=='side':return abs(normal[1])<=HORIZONTAL
    value=query.scope_normal[1]
    if name=='vertical':return abs(value)<=VERTICAL
    if name=='horizontal':return abs(value)>HORIZONTAL
    if name=='aslant':return VERTICAL<value<=HORIZONTAL
    if name=='nutant':return -HORIZONTAL<value<=NUTANT_HIGH
    if name=='border':return query.border
    if name=='inside':return not query.border
    raise ValueError('Unknown face component selector: '+name)


def select(value):
    if isinstance(value,Selector):return value
    if isinstance(value,(bool,np.bool_)):return Selector(str(value).lower(),lambda _:bool(value))
    if isinstance(value,Real):
        number=scalar(value)
        if number!=int(number):raise ValueError('A component index must be an integer-valued number')
        return Selector(str(int(number)),lambda query:query.index==int(number))
    if callable(value):return Selector(getattr(value,'__name__','python-predicate'),value)
    if value=='all':return Selector('all',lambda _:True)
    valid=set(AXES)|{'side','vertical','horizontal','aslant','nutant','border','inside'}
    valid|={'object.'+name for name in (*AXES,'side')}|{'world.'+name for name in WORLD}|{'street.'+name for name in ('front','back','left','right','side')}
    if not isinstance(value,str) or value not in valid:raise ValueError('Unknown face component selector: '+str(value))
    return Selector(value,lambda query:oriented(query,value))


def tagged(query,subcomponent=None):
    if not isinstance(query,str) or not query or subcomponent not in (None,'e','v'):
        raise ValueError('A tag query needs a nonempty string and optional e/v subcomponent')
    parts=query.split('.')
    if any(not part or (('*' in part or '?' in part) and part not in ('*','?')) for part in parts):
        raise ValueError('Tag wildcards replace whole dot-separated subtags')
    pattern='\\.'.join('[^.]+(?:\\.[^.]+)*' if part=='*' else '[^.]+' if part=='?' else re.escape(part) for part in parts)
    expression=re.compile('^'+pattern+'(?:\\.[^.]+)*$')
    def matches(face):
        tags=face.face.tags if subcomponent is None else face.edge_tags if subcomponent=='e' else face.vertex_tags
        return any(expression.fullmatch(tag) is not None for tag in tags)
    name='isTagged('+repr(query)+((', '+subcomponent) if subcomponent else '')+')'
    return Selector(name,matches)
