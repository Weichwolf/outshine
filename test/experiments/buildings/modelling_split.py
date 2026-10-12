"""Metric closed-solid Cartesian splits with explicit cut-face appearance and source ownership."""

from dataclasses import dataclass,replace

import numpy as np

from modelling_component_geometry import fit_scope
from modelling_cut_manifold import encoded_source
from modelling_cut_geos import cut_geos
from modelling_faces import Face
from modelling_metadata import inherit,group_id
from modelling_scope import scalar
from modelling_shape import Shape
from modelling_split_sizes import allocations


@dataclass(frozen=True)
class SplitInfo:
    index: int
    total: int
    pattern_index: int
    label: str
    axis: str
    interval: tuple
    source_faces: tuple
    omitted: tuple=()


def split(shape,axis,parts,repeat=False,adjust=True,cut_material=None,cut_group=0,max_parts=10000):
    if axis not in ('x','y','z'):raise ValueError('Cartesian split axis must be x, y or z')
    if not isinstance(adjust,bool):raise ValueError('Scope adjustment must be boolean')
    cut_group=group_id(cut_group)
    if cut_material is not None:Face((0,1,2),material=cut_material)
    dimension=('x','y','z').index(axis);size=scalar(shape.scope.size[dimension]);sign=1. if size>=0 else -1.
    spans=allocations(abs(size),parts,repeat,max_parts)
    if not spans:return ()
    source=encoded_source(shape);results=[]
    for span in spans:
        decoded,discarded=cut_geos(source,dimension,span.start,span.stop,cut_material,cut_group)
        omitted=tuple(((index,),0.,0.,'coplanar source surface outside retained volume') for index in discarded)
        delta=np.zeros(3);delta[dimension]=span.start*sign;scope=shape.scope.t(*delta)
        sizes=list(scope.size);sizes[dimension]=(span.stop-span.start)*sign;scope=replace(scope,size=tuple(sizes))
        if decoded is None:
            result=Shape.from_mesh(dict(vertices=np.empty((0,3)),faces=np.empty((0,3),dtype=np.int32)),scope)
            source_faces=()
        else:
            mesh,polygons,source_faces=decoded
            if adjust:scope=fit_scope(mesh['vertices'],scope.axes,scope.origin,scope,signed=True)
            result=Shape.from_mesh(mesh,scope,polygons)
        info=SplitInfo(span.index,span.total,span.pattern_index,span.label,axis,(span.start,span.stop),source_faces,omitted)
        results.append(replace(inherit(result,shape),comp_info=shape.comp_info,split_info=info))
    return tuple(results)
