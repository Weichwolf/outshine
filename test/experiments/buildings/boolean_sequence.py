"""Explicit leaf identities and immutable recursive triangle ancestry for CSG chains."""

from dataclasses import dataclass
from functools import cached_property

from boolean_surface_probe import boolean
from boolean_semantic_overlay import overlay
from boolean_shape_probe import assemble


@dataclass(frozen=True,order=True)
class Reference:
    source: str
    face: int
    triangle: int


@dataclass(frozen=True)
class Leaf:
    identifier: str
    shape: object


@dataclass(frozen=True)
class Operand:
    shape: object
    origins: tuple
    preferred: tuple
    leaves: tuple
    parent: object=None


@dataclass(frozen=True)
class Operation:
    identifier: str
    operation: str
    parents: tuple
    product: object
    origins: tuple
    preferred: tuple
    leaves: tuple

    @cached_property
    def operand(self):
        return Operand(self.product.shape,self.origins,self.preferred,self.leaves,self)


def identifier(value):
    if not isinstance(value,str) or not value.strip():raise ValueError('CSG graph identity needs a nonempty explicit string')
    return value


def leaf(name,shape):
    name=identifier(name);source=Leaf(name,shape)
    mesh=shape.emit();parts=mesh.get('parts')
    if parts is None or len(parts)!=len(mesh['faces']):raise ValueError('CSG leaf needs original logical face identities')
    references=tuple(Reference(name,int(part),number) for number,part in enumerate(parts))
    return Operand(shape,tuple((reference,) for reference in references),references,(source,))


def combine(name,a,b,operation='union'):
    name=identifier(name)
    if not isinstance(a,Operand) or not isinstance(b,Operand):raise ValueError('CSG sequence needs declared operands')
    parents=(a,b);sources={}
    for parent in parents:
        if len(parent.origins)!=len(parent.shape.geometry.fields['faces']) or len(parent.preferred)!=len(parent.origins):
            raise ValueError('CSG operand triangle lineage is incomplete')
        for source in parent.leaves:
            if source.identifier in sources and sources[source.identifier] is not source:
                raise ValueError('Duplicate CSG leaf identity; reuse the original declared leaf')
            sources[source.identifier]=source
    for parent in parents:
        for origins,preferred in zip(parent.origins,parent.preferred,strict=True):
            if preferred not in origins:raise ValueError('CSG preferred origin is absent from complete ancestry')
            for reference in origins:
                source=sources.get(reference.source)
                if source is None:raise ValueError('CSG ancestor references an undeclared leaf')
                parts=source.shape.geometry.fields.get('parts',())
                if not 0<=reference.triangle<len(parts) or int(parts[reference.triangle])!=reference.face:
                    raise ValueError('CSG ancestor has an invalid original triangle/face')
    result=overlay(boolean(a.shape,b.shape,operation),method='exact-output');product=assemble(result)
    origins=[];preferred=[]
    for supports in result.supports:
        inherited=tuple(sorted({reference for owner,triangle in supports for reference in parents[owner].origins[triangle]}))
        owner,triangle=supports[0];selected=parents[owner].preferred[triangle]
        if selected not in inherited:raise ValueError('Preferred CSG source left complete recursive ancestry')
        origins.append(inherited);preferred.append(selected)
    return Operation(name,operation,parents,product,tuple(origins),tuple(preferred),tuple(sources[key] for key in sorted(sources)))
