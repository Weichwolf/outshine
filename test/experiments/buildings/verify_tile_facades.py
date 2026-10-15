"""Source envelope/UV binding and independent projected filter footprints."""

import argparse
import copy
import json
from pathlib import Path
import resource
import time

import drjit as dr
import mitsuba as mi
import numpy as np
import shapely

from demand import View
from evidence import revision
from mitsuba_facade import bind,material,validate
from mitsuba_mesh import mesh as physical_mesh
from provider_contract import attributes
from showcase_tiles import view_for
from tile_buildings import SITES,read_tile,prism,construct
from tile_facades import project,contexts,FacadeDetails
from outshine_ref.buildings.facades import rectangle_coverage,FacadeRepeat


def footprint_reference(view,points,a,tangent,normal,cell_size):
    result=[]
    for point in points:
        p=(point-view.eye)@view.basis.T;uv=p[:2]*view.focal_px/p[2]+[view.width/2,view.height/2]
        def intersect(pixel):
            ray=view.basis[2]+view.basis[0]*(pixel[0]-view.width/2)/view.focal_px+view.basis[1]*(pixel[1]-view.height/2)/view.focal_px
            return view.eye+ray*((a-view.eye)@normal/(ray@normal))
        derivatives=[]
        for axis in range(2):
            offset=np.zeros(2);offset[axis]=1e-3
            value=(intersect(uv+offset)-intersect(uv-offset))/(2e-3)
            derivatives.append([value@tangent/cell_size[0],value[2]/cell_size[1]])
        result.append(np.abs(derivatives).sum(axis=0))
    return np.asarray(result)


def shader_controls():
    mi.set_variant('llvm_ad_rgb');dr.set_thread_count(2);bind()
    row=dict(id='shader-wall',polygon=shapely.box(0,0,20,10),properties=dict(render_height=15.,render_min_height=0.))
    p=attributes(row['properties']);original,_=prism(row,p,12)
    original['uv']=np.c_[original['vertices'][:,0]*.17,original['vertices'][:,1]*.13]
    mesh,selected,facades=project(original,row,p,(.48,.53),3.2,3.)
    np.testing.assert_array_equal(mesh['uv'][mesh['faces'][~selected]],original['uv'][original['faces'][~selected]])
    mesh['materials'][selected]=32768
    spec={'32768':dict(fractions=[.48,.53],wall_material=12)};validate(mesh,spec)
    for value in (dict(mesh,facade_extent=np.zeros_like(mesh['facade_extent'])),{k:v for k,v in mesh.items() if k!='facade_extent'}):
        try:validate(value,spec)
        except ValueError:pass
        else:raise ValueError('Missing/zero actual facade binding was accepted')
    np.testing.assert_array_equal(mesh['vertices'][mesh['faces']],original['vertices'][original['faces']])
    records=[]
    xyz=np.array([(x,0.,z) for x in (1.,2.5,8.3333333,10.,17.5,19.) for z in (1.,4.5,7.5,9.,13.5)])
    for eye in ([10.,-30.,8.],[10.,-1500.,8.],[-80.,-5.,12.]):
        config=dict(eye=eye,target=[10.,0.,7.5],profile=[1920,1080],fov_deg=48.)
        bsdf,_=material(dict(fractions=[.48,.53],wall_material=12),config,dict(type='diffuse',reflectance=.6))
        shape,proof=physical_mesh('wall',mesh,np.arange(len(mesh['faces'])),bsdf)
        scene=mi.load_dict(dict(type='scene',wall=shape));directions=xyz-np.asarray(eye)
        ray=mi.Ray3f(mi.Point3f(eye),mi.Vector3f(directions.T));si=scene.ray_intersect(ray)
        if not bool(dr.all(si.is_valid())):raise ValueError('Shader control ray missed its actual wall')
        texture=mi.load_dict(bsdf['weight']);values=np.asarray(texture.eval_1(si));foot=np.asarray(texture.footprint(si)).T
        view=View(np.asarray(eye),np.asarray(config['target']),width=1920,height=1080,fov_deg=48.)
        wanted=footprint_reference(view,xyz,np.zeros(3),np.array([1.,0.,0.]),np.array([0.,-1.,0.]),[20/6,3.])
        np.testing.assert_allclose(foot,wanted,atol=2e-5,rtol=2e-5)
        uv=np.asarray(si.uv).T;extent=np.asarray(shape.eval_attribute_3('vertex_facade_extent',si)).T
        np.testing.assert_allclose(extent,np.tile([6,5,0],(len(xyz),1)),atol=1e-6,rtol=0)
        expected=rectangle_coverage(uv,foot,[.48,.53],[6,5]);np.testing.assert_allclose(values,expected,atol=2e-5,rtol=0)
        if not np.any(values>.1):raise ValueError('Correct physical facade binding produced no window response')
        wrong=shape.eval_attribute_3('facade_extent',si)
        np.testing.assert_array_equal(np.asarray(wrong),0.)
        records.append(dict(eye=eye,rays=len(xyz),max_coverage_error=float(abs(values-expected).max()),
            max_footprint_error=float(abs(foot-wanted).max()),**proof))
    return records


def costs(repeats=21):
    from verify import coverage_reference
    results=[];rng=np.random.default_rng(314);centres=rng.uniform([-2,-2],[50,22],(512,2));feet=rng.uniform(.01,7,(512,2))
    for columns,rows in ((1,1),(13,6),(35,17)):
        layout=FacadeRepeat(47.3,19.4,columns,rows,.48,.53);pw,ph=layout.cell_size_m
        rectangles=np.array([[(c+.5)*pw-pw*.48/2,(r+.5)*ph-ph*.53/2,(c+.5)*pw+pw*.48/2,(r+.5)*ph+ph*.53/2]
                             for r in range(rows) for c in range(columns)])
        expected=coverage_reference(rectangles,centres,feet);actual=layout.coverage(centres,feet)
        np.testing.assert_allclose(actual,expected,atol=1e-12,rtol=1e-10)
        phases={'compact':[],'expanded-rectangles':[]}
        for index in range(repeats+2):
            for method in phases:
                began=time.perf_counter()
                layout.coverage(centres,feet) if method=='compact' else coverage_reference(rectangles,centres,feet)
                if index>=2:phases[method].append((time.perf_counter()-began)*1000)
        results.append(dict(columns=columns,rows=rows,boxes=512,opened_rectangles=len(rectangles),
            phases={key:dict(samples=repeats,p50_ms=float(np.percentile(values,50)),p95_ms=float(np.percentile(values,95))) for key,values in phases.items()}))
    return results


def source_controls():
    rows=[];receipts=[]
    for site in SITES:
        source,receipt=read_tile(site);before=[(r['id'],r['polygon'].wkb,copy.deepcopy(r['properties'])) for r in source]
        context=contexts(source,receipt);details=FacadeDetails(source,view_for(source))
        mesh,report=construct(site,prepared=(source,receipt),ordinary_builder=details)
        if report['generated']+len(report['omissions'])!=report['delivered_building_polygons']:raise ValueError('Facade experiment lost source accounting')
        if len(mesh.get('facade_extent',()))!=len(mesh['vertices']):raise ValueError('Facade attribute does not match emitted vertex ownership')
        for row,(identifier,polygon,properties) in zip(source,before,strict=True):
            assert row['id']==identifier and row['polygon'].wkb==polygon and row['properties']==properties
        cases=details.completed
        if details.rejected:raise ValueError('Complete facade frames rejected: '+str(details.rejected[:3]))
        for row in cases:
            for f in row['facades']:
                repeat=FacadeRepeat(f['width_m'],f['height_m'],f['columns'],f['rows'],*f['fractions'])
                np.testing.assert_allclose(repeat.coverage([f['width_m']/2,f['height_m']/2],[f['width_m'],f['height_m']]),np.prod(f['fractions']),atol=1e-12,rtol=0)
        rows.append(dict(site=site,bodies=report['generated'],omissions=len(report['omissions']),facade_bodies=len(cases),
            facade_planes=sum(len(c['facades']) for c in cases),families=details.report()['families'],
            estimated_openings=sum(f['opening_count'] for c in cases for f in c['facades']),
            binding_templates=len(details.bindings),source_properties_unchanged=True,context=context))
        receipts.append(receipt['receipt']);print(site,'pass',len(cases),'facade bodies',flush=True)
    return rows,receipts


def run(output):
    output.parent.mkdir(parents=True,exist_ok=True);began=time.perf_counter();shader=shader_controls();rows,receipts=source_controls()
    result=revision()|dict(status='pass',shader_controls=shader,cases=rows,receipts=receipts,costs=costs(),seconds=time.perf_counter()-began,
        process_peak_rss_mib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/1024,
        scope='Independent actual UV/frame/attribute/finite-filter and source checks; current-camera projected tangent box is a reference approximation, not path-ray EWA, actual near rooms or native frame/device admission.')
    output.write_text(json.dumps(result,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/facade-verification.json'))
    run(parser.parse_args().output)
