"""Check closed roof surfaces, source envelopes, body access and alternative roof constructions."""

import argparse
import json
from pathlib import Path
import time

import numpy as np
import shapely
from shapely.affinity import rotate
import py_straight_skeleton.algorithm as skeleton_algorithm

from building_plan_cases import cases
from building_vertical import mesh as body_mesh
from evidence import revision
from roof_components import ROOF_MATERIAL,THICKNESS_M,mesh as roof_mesh
from roof_planes import build as planes,height,skeleton
from roof_recipe import build,mesh,parameters
from verify_layout import closed_parts
from verify_vertical import verify as verify_body


def exterior_overlap(plan, product):
    triangles = product['vertices'][product['faces']]
    selected = triangles.max(axis=1)[:,2]>=plan['body_top_m']-THICKNESS_M-1e-7
    triangles,parts = triangles[selected],product['parts'][selected]
    total = 0.
    for ring in [plan['footprint'].exterior,*plan['footprint'].interiors]:
        points = np.array(ring.coords)
        for a,b in zip(points,points[1:]):
            tangent = (b-a)/np.linalg.norm(b-a)
            normal = np.array([-tangent[1],tangent[0]])
            offsets = triangles[:,:,:2]-a
            on_plane = np.max(abs(offsets@normal),axis=1)<1e-7
            plane_faces = triangles[on_plane]
            ids = parts[on_plane]
            clip = shapely.box(0.,plan['body_top_m']-THICKNESS_M-.001,np.linalg.norm(b-a),plan['source_top_m']+.001)
            shapes = {}
            for face,identifier in zip(plane_faces,ids):
                polygon = shapely.Polygon(np.c_[(face[:,:2]-a)@tangent,face[:,2]]).intersection(clip)
                if polygon.area>1e-10:
                    shapes.setdefault(int(identifier),[]).append(polygon)
            surfaces = [shapely.union_all(polygons) for polygons in shapes.values()]
            union = shapely.union_all(surfaces)
            overlap = sum(surface.area for surface in surfaces)-union.area
            if overlap>1e-6:
                raise AssertionError('Distinct roof/body parts have coplanar exterior overlap')
            total += max(0.,overlap)
    return total


def verify(plan, product):
    if plan['roof_status']!='connected-roof':
        raise AssertionError('No constructive roof/body plan')
    bare = body_mesh(plan)
    access = verify_body(plan,bare)
    closed_parts(product)
    overlap = exterior_overlap(plan,product)
    referenced = product['vertices'][np.unique(product['faces'])]
    bounds = referenced[:,2].min(),referenced[:,2].max()
    if not np.allclose(bounds,[plan['base_z_m'],plan['source_top_m']],atol=1e-7,rtol=0):
        raise AssertionError('Roof altered the delivered total building-height interval')
    triangles = product['vertices'][product['faces']]
    normals = np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0])
    selected = (product['materials']==ROOF_MATERIAL)&(normals[:,2]>1e-10)
    surfaces = [shapely.Polygon(face[:,:2]) for face in triangles[selected]]
    cover = shapely.union_all(surfaces)
    if cover.symmetric_difference(plan['footprint']).area>1e-6 or sum(p.area for p in surfaces)-cover.area>1e-6:
        raise AssertionError('Actual roof coverage leaves a gap, overlaps or fills a source courtyard')
    if plan['roof']['kind']!='flat':
        extra = product['faces'][product['parts']>int(bare['parts'].max())]
        if product['vertices'][np.unique(extra),2].min()<plan['ceiling_z_m']+(plan['body_top_m']-plan['base_z_m'])*(plan['storeys']-1)/plan['storeys']-1e-7:
            raise AssertionError('Roof or attic infill penetrates occupied top-storey headroom')
    return access|dict(source_height_preserved=True,source_courtyards_preserved=True,
                        roof_facets=len(plan['roof']['facets']),roof_method=plan['roof']['method'],
                        roof_rise_m=plan['roof']['rise_m'],body_top_m=plan['body_top_m'],exterior_coplanar_overlap_m2=overlap)


def compare():
    rows = []
    for width,depth in ((14.,10.),(8.,7.),(12.,12.)):
        shape = rotate(shapely.box(0,0,width,depth),31.)
        results = []
        for method in ('analytic-rectangle','straight-skeleton'):
            timings = []
            for repeat in range(22):
                start = time.perf_counter()
                roof = planes(shape,'hip',method)
                if repeat:
                    timings.append((time.perf_counter()-start)*1000)
            product = roof_mesh(roof,shape,6.,1.6)
            roof_only = {key:array if key=='vertices' else array[product['materials']==ROOF_MATERIAL] for key,array in product.items()}
            volume = closed_parts(roof_only)
            if abs(volume-shape.area*THICKNESS_M)>1e-6:
                raise AssertionError('Roof slabs changed the analytic area times vertical thickness')
            rows.append(dict(width_m=width,depth_m=depth,method=method,p50_ms=float(np.median(timings)),
                             p95_ms=float(np.quantile(timings,.95)),repetitions=len(timings),facets=len(roof['facets']),
                             triangles=len(roof_only['faces']),closed_slab_volume_m3=volume))
            results.append(roof)
        coordinates = np.array(shape.exterior.coords)[:4]
        for x in np.linspace(.02,.98,19):
            for y in np.linspace(.02,.98,19):
                point = coordinates[0]+x*(coordinates[1]-coordinates[0])+y*(coordinates[3]-coordinates[0])
                values = [height(next(f for f in roof['facets'] if f['polygon'].buffer(1e-8).covers(shapely.Point(point))),point,roof['origin'])
                          for roof in results]
                if abs(values[0]-values[1])>1e-7:
                    raise AssertionError('Analytic and skeleton roofs disagree on a sampled surface height')
    return rows


def controls():
    shape = shapely.box(0,0,14,10)
    previous = skeleton_algorithm.GLOBAL_ALGORITHM_TRACER
    try:
        skeleton(shape,events=0)
    except RuntimeError as error:
        if 'event budget' not in str(error):
            raise
    else:
        raise AssertionError('Roof skeleton ignored its event cap')
    if skeleton_algorithm.GLOBAL_ALGORITHM_TRACER is not previous:
        raise AssertionError('A failed roof skeleton changed the shared library tracer')
    planes(shape,'hip','straight-skeleton')
    sample = dict(id='roof-tags',polygon=shape,properties={'render_height':9.,'building:levels':2,
                                                       'roof:shape':'gabled','roof:height':1.5,'roof:colour':'#794533'})
    plan = build(sample)
    if plan['storeys']!=2 or plan['source_properties']!=sample['properties'] or plan['body_top_m']!=7.5:
        raise AssertionError('Roof reservation changed supplied storeys or source attributes')
    verify(plan,mesh(plan))
    bare,complete = body_mesh(plan),mesh(plan)
    ceiling = (bare['materials']==2)&(bare['levels']==plan['storeys']-1)
    duplicated = complete|dict(faces=np.r_[complete['faces'],bare['faces'][ceiling]],
                                parts=np.r_[complete['parts'],bare['parts'][ceiling]])
    try:
        exterior_overlap(plan,duplicated)
    except AssertionError:
        pass
    else:
        raise AssertionError('Duplicating the original roof-edge ceiling passed overlap verification')
    try:
        parameters(sample,'hip')
    except ValueError:
        pass
    else:
        raise AssertionError('A recipe silently replaced a supplied roof shape')
    angled = sample|dict(properties={'render_height':12.,'building:levels':2,'roof:shape':'hipped','roof:angle':25.})
    shaped = build(angled)
    if abs(shaped['roof']['rise_m']-5*np.tan(np.deg2rad(25.)))>1e-7:
        raise AssertionError('Supplied roof angle did not control the actual source-width profile')
    verify(shaped,mesh(shaped))
    tight = sample|dict(properties={'render_height':6.,'building:levels':2,'roof:shape':'hipped'})
    tight_plan = build(tight)
    verify(tight_plan,mesh(tight_plan))
    try:
        parameters(sample|dict(properties=sample['properties']|{'hide_3d':True}))
    except ValueError as error:
        if 'hide_3d' not in str(error):
            raise
    else:
        raise AssertionError('A provider-suppressed building reached roof generation')
    return dict(event_cap=True,tracer_restored_after_failure=True,supplied_tags_and_levels=True,
                source_conflict_rejected=True,supplied_angle=True,provider_suppression=True,
                compact_supplied_storeys_under_roof=True,old_ceiling_overlap_rejected=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/roof-validation.json'))
    parser.add_argument('--cases',nargs='+')
    args = parser.parse_args()
    results,omitted = [],[]
    for sample in cases():
        if args.cases is not None and sample['label'] not in args.cases:
            continue
        for kind in ('flat','hip','gable','shed'):
            try:
                plan = build(sample,kind)
            except (RuntimeError,ValueError) as error:
                omitted.append(dict(label=sample['label'],kind=kind,reason=str(error)))
                continue
            if plan['roof_status']=='unplanned':
                omitted.append(dict(label=sample['label'],kind=kind,reasons=plan['omissions']))
                continue
            checks = verify(plan,mesh(plan))
            results.append(checks|dict(label=sample['label'],kind=kind))
            print(json.dumps(dict(case=sample['label'],kind=kind,storeys=plan['storeys'],facets=checks['roof_facets'])),flush=True)
    if args.cases is None and (len(results)!=32 or len(omitted)!=12):
        raise AssertionError('Roof/body fixtures lost complete constructive coverage')
    result = dict(**revision(),cases=results,omissions=omitted,controls=controls(),comparison=compare(),
                  scope='Closed roof/body components and actual source envelope; generic interiors, no native LOD or completed architecture family')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(status='pass',constructive=len(results),omissions=len(omitted))))


if __name__=='__main__':
    main()
