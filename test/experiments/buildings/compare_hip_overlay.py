"""Compare equally checked global noding and pair-intersection roof overlays."""

import importlib
import json
from pathlib import Path

import numpy as np
import shapely

from evidence import revision
from outshine_ref.buildings.modelling.facet_overlay import domains, overlay, triangulate_cells
from outshine_ref.buildings.modelling.faces import plane
from outshine_ref.buildings.modelling import prepare_hip
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


def cases():
    result=[]
    for site in ('Vienna','Tokyo','Rosenheim'):
        rows,_=read_site(site)
        for row in sorted(rows,key=lambda s:len(s['polygon'].exterior.coords),reverse=True)[:16]:
            shape=source(row['polygon']);mesh=shape.emit();normal,polygon,_,face=plane(mesh['vertices'],shape.geometry.polygons[0])
            origin=mesh['vertices'][face.outer[0]];vertices=mesh['vertices'][list(face.outer)]
            edges=np.roll(vertices,-1,axis=0)-vertices;tangent=edges[np.argmax(np.linalg.norm(edges,axis=1))]
            tangent-=normal*(tangent@normal);tangent/=np.linalg.norm(tangent)
            plan=prepare_hip(polygon)
            projected=(mesh['vertices'][mesh['faces']]-origin)@np.array([tangent,np.cross(normal,tangent)]).T-plan.origin
            result.append((projected,plan))
    return result


if __name__=='__main__':
    module=importlib.import_module('outshine_ref.buildings.modelling.roof_hip')
    correctness={}
    for name,function in (('global-noding',overlay),('pair-intersections',pair_overlay)):
        module.overlay=function
        try:
            correctness[name]=dict(controls=len(controls()),real_cases=len(real_inputs()),status='pass')
        finally:module.overlay=overlay
    prepared=cases()
    calls={name:(lambda function=function:[function(corners,plan) for corners,plan in prepared])
           for name,function in (('global-noding',overlay),('pair-intersections',pair_overlay))}
    record=revision()|dict(status='pass',correctness=correctness,costs=costs(calls),batch_cases=len(prepared),
                          scope='Same 48 prepared native triangle/hip-plan inputs; overlay only, excludes acquisition, plan solving, lifting and render frames. Python tracked peak excludes GEOS/native heaps.')
    target=Path('build/experiments/buildings/hip-overlay-comparison.json')
    target.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps({key:record[key] for key in ('commit','status','correctness','costs','batch_cases','scope')}))
