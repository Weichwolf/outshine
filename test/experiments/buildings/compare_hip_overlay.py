"""Compare equally checked global noding and pair-intersection roof overlays."""

import importlib
import argparse
import math
from numbers import Real
import json
from pathlib import Path

import numpy as np
import shapely

from evidence import revision
from outshine_ref.buildings.modelling.facet_overlay import domains, overlay, triangulate_cells
from outshine_ref.buildings.modelling.faces import plane
from outshine_ref.buildings.modelling import SkeletonLimits,prepare_hip
from outshine_ref.buildings.modelling.hip_surface import prepare_even
from outshine_ref.buildings.modelling import scope
from samples import read_site
from verify_modelling_roof_hip import controls, real_inputs
from verify_modelling_roof_shed import source
from verify_scope_library import costs


def pair_overlay(corners,plan,limit=65536):
    original,facets,precision=domains(corners,plan,limit)
    pairs=shapely.intersection(original[:,None],facets[None,:],grid_size=precision).ravel()
    pairs=shapely.get_parts(pairs)
    polygons=pairs[(shapely.get_type_id(pairs)==3)&(shapely.area(pairs)>0)]
    network=shapely.union_all(shapely.boundary(polygons),grid_size=precision)
    cells=shapely.get_parts(shapely.polygonize(shapely.get_parts(network)))
    cells=cells[shapely.covers(plan.footprint,shapely.point_on_surface(cells))]
    return triangulate_cells(cells,original,facets,limit,precision)


def cases(even=False,shapes=False):
    result=[]
    for site in ('Vienna','Tokyo','Rosenheim'):
        rows,_=read_site(site)
        for row in sorted(rows,key=lambda s:len(s['polygon'].exterior.coords),reverse=True)[:16]:
            shape=source(row['polygon'])
            if shapes:result.append(shape);continue
            mesh=shape.emit();normal,polygon,_,face=plane(mesh['vertices'],shape.geometry.polygons[0])
            origin=mesh['vertices'][face.outer[0]];vertices=mesh['vertices'][list(face.outer)]
            edges=np.roll(vertices,-1,axis=0)-vertices;tangent=edges[np.argmax(np.linalg.norm(edges,axis=1))]
            tangent-=normal*(tangent@normal);tangent/=np.linalg.norm(tangent)
            plan=prepare_hip(polygon)
            projected=(mesh['vertices'][mesh['faces']]-origin)@np.array([tangent,np.cross(normal,tangent)]).T-plan.origin
            result.append((projected,prepare_even(plan,'byHeight') if even else plan))
    return result


def shape_batch(module,function,prepared,even):
    original=module.overlay
    try:
        module.overlay=function
        return [shape.roof_hip(3.,'byHeight',even=even,limits=SkeletonLimits(seconds=30.)) for shape in prepared]
    finally:module.overlay=original


def generic_scalar(value):
    if isinstance(value,bool) or not isinstance(value,Real) or not math.isfinite(value):
        raise ValueError('Scope parameters require finite real numbers')
    return float(value)


def scalar_batch(code,module,prepared,even):
    original=scope.scalar.__code__
    try:
        scope.scalar.__code__=code
        return shape_batch(module,overlay,prepared,even)
    finally:scope.scalar.__code__=original


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--even',action='store_true')
    parser.add_argument('--shapes',action='store_true')
    parser.add_argument('--scalar-baseline',action='store_true')
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/hip-overlay-comparison.json'))
    args=parser.parse_args()
    if args.scalar_baseline and not args.shapes:parser.error('--scalar-baseline requires --shapes')
    module=importlib.import_module('outshine_ref.buildings.modelling.roof_hip')
    correctness={};original_code=scope.scalar.__code__
    variants=([(name,overlay,code) for name,code in (('fast-numeric-types',original_code),('generic-real',generic_scalar.__code__))]
              if args.scalar_baseline else [(name,function,original_code) for name,function in (('global-noding',overlay),('pair-intersections',pair_overlay))])
    for name,function,code in variants:
        module.overlay=function;scope.scalar.__code__=code
        try:
            correctness[name]=dict(controls=len(controls(args.even)),real_cases=len(real_inputs(args.even)),status='pass')
        finally:module.overlay=overlay;scope.scalar.__code__=original_code
    prepared=cases(args.even,args.shapes)
    calls={name:((lambda function=function:shape_batch(module,function,prepared,args.even)) if args.shapes else
                 (lambda function=function:[function(corners,plan) for corners,plan in prepared]))
           for name,function in (('global-noding',overlay),('pair-intersections',pair_overlay))}
    if args.scalar_baseline:
        calls={name:(lambda code=code:scalar_batch(code,module,prepared,args.even))
               for name,code in (('fast-numeric-types',scope.scalar.__code__),('generic-real',generic_scalar.__code__))}
    record=revision()|dict(status='pass',even=args.even,correctness=correctness,costs=costs(calls),batch_cases=len(prepared),
                          scope=('Same 48 prepared source Shapes; full roof operation includes skeleton planning, even preparation, native overlay/lift, scope and metadata. Scalar baseline changes the shared function code so every imported alias follows the same parser. Diagnostic cooperative limit 30s permits tracer overhead; default 2s verified by correctness checks.' if args.shapes else
                                 'Same 48 prepared native triangle/hip-plan inputs; overlay only, excludes acquisition, plan solving and lifting.')
                                +' Acquisition and render frames excluded. Python tracked peak excludes GEOS/native heaps.')
    target=args.output
    target.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps({key:record[key] for key in ('commit','status','correctness','costs','batch_cases','scope')}))
