"""Verify constructive roadbeds against real mesh owners, sources and frozen routes."""

import argparse
from collections import Counter
import json
import math
from pathlib import Path
import subprocess
import time

import numpy as np
import shapely

from geos_triangulation import triangles_of
from infrastructure_building_clearance import features,polygon_parts,GIRTH_M
from infrastructure_cut_setback_experiment import prepared_network,digest,implementation_files
from infrastructure_cut_setbacks import restore_cut_plan
from infrastructure_directed_routes import directed_module_routes
from infrastructure_flat_surfaces import shared_mesh
from infrastructure_junction_surfaces import motor_surface
from infrastructure_route_connectivity import network_connectivity
from infrastructure_turn_roadbeds import compose_roadbeds,construct_turn_roadbed,preserves_voids,preserves_coverage
from infrastructure_travel import MODES
from infrastructure_vehicle_sweep import body_outlines,turn_body_check,turn_sampler


def building_reservations(receipt,region):
    lat,lon=receipt['origin']
    scale=math.cos(math.radians(lat))
    anchor=np.array([lon/360*GIRTH_M,math.asinh(math.tan(math.radians(lat)))*GIRTH_M/(2*math.pi)])
    buildings=[]
    read=[]
    for tile in receipt['source_tiles']:
        z,x,y=tile['tile']
        span=GIRTH_M/(1<<z)
        corner=(np.array([x*span,-y*span])+[-GIRTH_M/2,GIRTH_M/2]-anchor)*scale
        bounds=shapely.box(corner[0],corner[1]-span*scale,corner[0]+span*scale,corner[1])
        if not bounds.intersects(region):
            continue
        data=Path(tile['path']).read_bytes()
        assert digest(Path(tile['path']))==tile['sha256']
        read.append(tile)
        for layer,extent,kind,props,parts in features(data):
            if layer!='building' or kind!=3:
                continue
            rings=[((np.asarray(p)/extent+[x,y])*[span,-span]+[-GIRTH_M/2,GIRTH_M/2]-anchor)*scale for p in parts]
            for polygon in polygon_parts(rings):
                if polygon.intersects(region):
                    buildings.append(polygon if polygon.is_valid else polygon.envelope)
    return shapely.union_all(buildings),read


def reservations(network,region):
    graph=network.graph
    ids=network.tree.query(shapely.buffer(region,float(graph['half_widths'].max())),predicate='intersects')
    ids=ids[~np.any(graph['travel_public'][ids]&MODES['motor']!=0,axis=1)]
    bands=shapely.buffer(graph['lines'][ids],graph['half_widths'][ids],quad_segs=2)
    return shapely.union_all(bands)


def mesh_coverage(surface,vehicle,fit,begin,end,error,profile='G2'):
    triangles=triangles_of(surface)
    actual=shapely.union_all(triangles)
    tolerance=max(1e-7,surface.area*1e-12)
    assert surface.symmetric_difference(actual).area<=tolerance
    assert abs(float(shapely.area(triangles).sum())-actual.area)<=tolerance
    body,_=turn_body_check(actual,fit,begin,end,profile,vehicle,error)
    assert body['fits_surface']
    xy,heading=turn_sampler(profile)(fit,np.linspace(0,1,2049))[:2]
    outlines=body_outlines(xy[0],heading[0],vehicle['widthM']*.5,
                          vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
    assert np.all(shapely.covers(actual,shapely.polygons(outlines)))
    return dict(triangles=len(triangles),continuous_body_on_mesh=body,dense_body_poses=2049),actual


def verify_inflected(vehicle):
    axis=shapely.LineString([[-8.,0.],[6.,0.],[6.,1.],[20.,1.]])
    floor=shapely.buffer(axis,1.1,quad_segs=8)
    source=shapely.LineString([[0.,0.],[6.,0.],[6.,1.],[12.,1.]])
    entry=dict(position=np.array([0.,0.]),heading=np.array([1.,0.]))
    exit=dict(position=np.array([12.,1.]),heading=np.array([1.,0.]))
    measured=[]
    for angle,offset in ((0.,[0.,0.]),(.7,[10000.,-7000.])):
        matrix=np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]])
        move=lambda xy:xy@matrix.T+offset
        surface=shapely.transform(floor,move)
        a,b=(dict(position=move(p['position']),heading=matrix@p['heading']) for p in (entry,exit))
        route=shapely.transform(source,move)
        candidate,_=construct_turn_roadbed(a,b,surface,shapely.Polygon(),route,vehicle)
        assert candidate is not None and candidate['report']['profile']=='G2-polynomial'
        fit=candidate['fit'];profile=candidate['report']['profile']
        old_body,_=turn_body_check(surface,fit,a['position'],b['position'],profile,vehicle)
        assert not old_body['fits_surface'] and candidate['addition'].area>0
        mesh_coverage(candidate['surface'],vehicle,fit,a['position'],b['position'],.025,profile)
        base,core,planned,physical,quantum=compose_roadbeds(surface,surface,[candidate['roadbed']])
        assert preserves_coverage(surface,planned)
        mesh_coverage(planned,vehicle,fit,a['position'],b['position'],.025,profile)
        blocked,_=construct_turn_roadbed(a,b,surface,shapely.buffer(surface,2).difference(surface),route,vehicle)
        assert blocked is None
        measured.append(candidate['addition'].area)
    assert abs(measured[0]-measured[1])<1e-7
    return dict(inflected_turns_checked=2,inflected_added_area_m2=measured[0])


def verify_small(vehicle):
    floor=shapely.union_all([shapely.box(-20,-1.75,20,1.75),shapely.box(-1.75,-1.75,1.75,20)])
    begin,finish=np.array([-7.,0.]),np.array([0.,7.])
    entry=dict(position=begin,heading=np.array([1.,0.]))
    exit=dict(position=finish,heading=np.array([0.,1.]))
    source=shapely.LineString([begin,[0.,0.],finish])
    measured=[]
    for angle,offset in ((0.,[0.,0.]),(.7,[10000.,-7000.])):
        matrix=np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]])
        move=lambda xy:xy@matrix.T+offset
        surface=shapely.transform(floor,move)
        a,b=(dict(position=move(p['position']),heading=matrix@p['heading']) for p in (entry,exit))
        route=shapely.transform(source,move)
        candidate,_=construct_turn_roadbed(a,b,surface,shapely.Polygon(),route,vehicle)
        assert candidate is not None
        profile=candidate['report']['profile']
        audit,_=mesh_coverage(candidate['surface'],vehicle,candidate['fit'],a['position'],b['position'],.025,profile)
        base,core,planned,physical,quantum=compose_roadbeds(surface,surface,[candidate['roadbed']])
        assert preserves_coverage(surface,planned)
        mesh_coverage(planned,vehicle,candidate['fit'],a['position'],b['position'],.025,profile)
        blocked,_=construct_turn_roadbed(a,b,surface,shapely.buffer(surface,2).difference(surface),route,vehicle)
        assert blocked is None
        assert preserves_coverage(surface,candidate['surface'])
        measured.append(candidate['addition'].area)
    assert abs(measured[0]-measured[1])<1e-7
    perforated=shapely.box(-5,-5,5,5).difference(shapely.box(-1,-1,1,1))
    assert not preserves_voids(perforated,shapely.box(-5,-5,5,5))
    assert preserves_voids(perforated,perforated.union(shapely.box(-1,-1,-.9,1)))
    rejected,trials=construct_turn_roadbed(entry,exit,floor,shapely.Polygon(),source,vehicle,1e-12)
    assert rejected is None and any(r.get('reason')=='roadbed_sampling_budget_exhausted' for r in trials)
    return dict(real_mesh_body_checked=True,rigid_transform_checked=True,reservations_enforced=True,
                hole_removal_rejected=True,local_block_edge_change_allowed=True,sampling_budget_enforced=True,
                composed_roadbeds_checked=True,**verify_inflected(vehicle))


def real_case(root,name,module):
    before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
    frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
    network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
    cut_path=root/'cut-setbacks'/f'{name}-cut-setbacks.json'
    cuts=json.loads(cut_path.read_text())
    restore_cut_plan(network,cuts['plan'])
    records={r['module']:r for r in before['modules']}
    records.update((r['module'],r) for r in cuts['changed_modules'])
    before=dict(before,modules=[records[r['module']] for r in before['modules']])
    changed_owners={r['module'] for r in cuts['plan']['parts']}
    checks=verify_small(network.vehicle)
    shape=network.shape(module)
    poses,_=network.poses(module,{})
    surface=motor_surface(shape,network.graph,network.tree,network.mesh,network.vehicle,
                          network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
    routes,_=directed_module_routes(network.graph,poses,shape,network.tree)
    previous=next(row for row in before['modules'] if row['module']==module)
    failed=[(r['entry'],r['exit']) for r in previous['turns'] if not r['accepted']]
    region=surface.envelope.buffer(network.vehicle['widthM']*.5)
    receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
    buildings,tiles=building_reservations(receipt,region)
    reserved=buildings.union(reservations(network,region).difference(surface))
    sections=np.array(list(network.sections.values()),dtype=object)
    section_index=shapely.STRtree(sections)
    rows,accepted=[],[]
    for entry,exit in failed:
        began=time.perf_counter()
        candidate,trials=construct_turn_roadbed(poses[entry],poses[exit],surface,reserved,
                    routes[entry,exit],network.vehicle,network.curbs['arc_error_m'])
        positions=np.array([poses[i]['position'] for i in (entry,exit)])
        headings=np.array([np.arctan2(poses[i]['heading'][1],poses[i]['heading'][0]) for i in (entry,exit)])
        row=dict(entry=entry,exit=exit,construction_ms=(time.perf_counter()-began)*1000,trials=trials,accepted=False,
                 source_path=shapely.get_coordinates(routes[entry,exit]).tolist(),
                 endpoint_bodies=body_outlines(positions,headings,network.vehicle['widthM']*.5,
                             network.vehicle['frontFromRearAxleM'],network.vehicle['rearOverhangM']).tolist())
        rows.append(row)
        if candidate is None:
            row['reason']='no_valid_constructive_turn'
            row['reasons']=dict(Counter(t['reason'] for t in trials))
            continue
        addition=candidate['addition']
        ids=network.mesh[1].query(addition,predicate='intersects')
        owners=set(map(int,network.product['face_owners'][ids]))|changed_owners
        if any(addition.intersection(network.shape(owner)).area>1e-8 for owner in owners if owner!=module):
            row['reason']='roadbed_overlaps_another_mesh_owner'
            continue
        planned_shape=shape.union(addition)
        if not preserves_voids(shape,planned_shape):
            row['reason']='roadbed_changes_physical_void_topology'
            continue
        nearby=section_index.query(shapely.buffer(addition,1e-7))
        if len(nearby) and np.any(shapely.distance(addition,sections[nearby])<1e-7):
            row['reason']='roadbed_touches_shared_cut'
            continue
        audit,actual=mesh_coverage(candidate['surface'],network.vehicle,candidate['fit'],
                                 poses[entry]['position'],poses[exit]['position'],network.curbs['arc_error_m'],
                                 candidate['report']['profile'])
        xy,heading=turn_sampler(candidate['report']['profile'])(candidate['fit'],np.linspace(0,1,65))[:2]
        bodies=body_outlines(xy[0],heading[0],network.vehicle['widthM']*.5,
                            network.vehicle['frontFromRearAxleM'],network.vehicle['rearOverhangM'])
        outside=shapely.area(shapely.difference(shapely.polygons(bodies),surface))
        row.update(accepted=True,reason='constructed_and_triangulated',audit=audit,report=candidate['report'],
                   path=shapely.get_coordinates(candidate['path']).tolist(),surface_wkb=actual.wkb.hex(),
                   physical_shape_wkb=planned_shape.wkb.hex(),addition_wkb=addition.wkb.hex(),
                   diagnostic_body_outline=bodies[np.argmax(outside)].tolist())
        accepted.append(candidate)
    base,core,planned,physical,coordinate_precision=compose_roadbeds(surface,shape,[c['roadbed'] for c in accepted])
    parts=shapely.get_parts(physical)
    roles=np.ones(len(parts),dtype=int)
    precision=json.loads((root/'directed-surfaces'/f'{name}-flat-surfaces.json').read_text())['precision_m']
    mesh,audit=shared_mesh(physical,parts,roles,precision)
    assert np.array_equal(mesh['vertices'],np.rint(mesh['vertices']/coordinate_precision)*coordinate_precision)
    emitted=shapely.polygons(mesh['vertices'][mesh['indices']])
    actual=shapely.union_all(shapely.set_precision(emitted,coordinate_precision))
    complete=actual.intersection(planned).union(base.difference(core))
    for candidate in accepted:
        pair=next((r['entry'],r['exit']) for r in rows if r.get('report') is candidate['report'])
        body,_=turn_body_check(complete,candidate['fit'],poses[pair[0]]['position'],poses[pair[1]]['position'],
                              candidate['report']['profile'],network.vehicle,network.curbs['arc_error_m'])
        if not body['fits_surface']:
            target=root/'roadbeds/publish-rejection'/f'{name}-{module}-{pair[0]}-{pair[1]}.json'
            target.parent.mkdir(parents=True,exist_ok=True)
            snapshot=dict(expected_wkb=candidate['surface'].wkb.hex(),planned_wkb=planned.wkb.hex(),
                          complete_wkb=complete.wkb.hex(),physical_wkb=physical.wkb.hex(),actual_wkb=actual.wkb.hex(),
                          fit=candidate['fit'],begin=poses[pair[0]]['position'],end=poses[pair[1]]['position'],
                          vehicle=network.vehicle,error=network.curbs['arc_error_m'],body=body)
            target.write_text(json.dumps(snapshot,default=lambda obj:obj.tolist())+'\n')
        assert body['fits_surface'],f'published physical mesh fails the continuous body check: {pair}'
    product=root/'roadbeds'/f'{name}-{module}-mesh.npz'
    product.parent.mkdir(parents=True,exist_ok=True)
    np.savez_compressed(product,vertices=mesh['vertices'],indices=mesh['indices'],
                        face_owners=np.full(len(emitted),module))
    proved={(r['entry'],r['exit']) for r in rows if r['accepted']}
    changed=dict(previous,accepted=previous['accepted']+len(proved),
                 turns=[dict(t,accepted=True) if (t['entry'],t['exit']) in proved else t for t in previous['turns']])
    after=dict(before,modules=[changed if r['module']==module else r for r in before['modules']])
    connectivity=dict(before=network_connectivity(network,before),after=network_connectivity(network,after))
    return dict(place=name,module=module,vehicle=network.vehicle,checks=checks,previous_accepted=previous['accepted'],
                accepted=previous['accepted']+len(accepted),connections=previous['connections'],
                source_tiles=tiles,previous_routes_preserved=bool(preserves_coverage(surface,planned)),
                added_area_m2=float(planned.area-surface.area),trials=rows,physical_mesh_audit=audit,
                physical_mesh=str(product),physical_mesh_sha256=digest(product),
                coordinate_precision_m=coordinate_precision,
                connectivity=connectivity,scope='One ground module; graph comparison uses its complete frozen one-km public-motor cohort.',
                original_surface_wkb=surface.wkb.hex(),physical_shape_wkb=shape.wkb.hex(),
                planned_surface_wkb=planned.wkb.hex(),input_sha256=before['input_sha256']|
                    {str(cut_path.relative_to(root)):digest(cut_path)})


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path('build/experiments/infrastructure'))
    parser.add_argument('--place',default='BaselBadischerBahnhof')
    parser.add_argument('--module',type=int,default=1109)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/infrastructure/roadbeds/roadbeds.json'))
    args=parser.parse_args()
    began=time.perf_counter()
    result=real_case(args.root,args.place,args.module)
    sources=implementation_files(Path(__file__))+[Path(__file__).with_name('geos_triangulation.py').resolve(),
              Path(__file__).with_name('infrastructure_network_recipes.json').resolve()]
    result.update(elapsed_ms=(time.perf_counter()-began)*1000,
                  measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
                  implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources})
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:result[k] for k in ('place','module','checks','previous_accepted','accepted','connections','previous_routes_preserved','added_area_m2','elapsed_ms')}),flush=True)
    print(json.dumps([dict(entry=r['entry'],exit=r['exit'],accepted=r['accepted'],reason=r.get('reason'),construction_ms=r['construction_ms']) for r in result['trials']]),flush=True)


if __name__=='__main__':
    main()
