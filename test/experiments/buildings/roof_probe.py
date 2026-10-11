"""Render roof/body variants through metric sections, spatial inspections and matched exterior cameras."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np

from building_plan_cases import cases
from building_plan_probe import encode,render_spatial
from demand import View
from esri_oracle import views
from evidence import revision
from facade_plan import plan as facade_plan,wall_colour
from preview import configuration,run_render
from roof_recipe import build,mesh
from roof_views import figure
from verify_roofs import verify


KINDS = ('flat','hip','gable','shed')
DEFAULT_CASES = ('rectangle','L-wing','courtyard','vienna-compact','vienna-courtyard','tokyo-compact')


def exterior_scene(sample, plan, mesh_path, output, view):
    reference = Path('build/experiments/buildings/esri-oracle')/f'esri-candler-{sample["label"]}-{view}-scene.json'
    if reference.is_file():
        config = json.loads(reference.read_text())
        config.pop('reference_obj',None)
        config.pop('reference_bounds_m',None)
        config.update(products=[],tiles={},solar_tile=None,output=str(output.resolve()))
        camera_origin = 'same pinned Esri-Candler camera and shared lighting'
    else:
        camera = views(facade_plan(sample))[view]
        camera = View(camera.eye,camera.target,width=1280,height=720)
        config,_ = configuration([],camera,True,output)
        camera_origin = 'same source-envelope camera construction as Esri-Candler oracle'
    colour = np.array(plan['roof']['colour'])
    linear = np.where(colour<=.04045,colour/12.92,((colour+.055)/1.055)**2.4)
    config['constructive'] = dict(mesh=str(mesh_path.resolve()),mesh_sha256=hashlib.sha256(mesh_path.read_bytes()).hexdigest(),
                                  wall_linear=wall_colour(sample['properties'],(1.,1.,1.)).tolist(),roof_linear=linear.tolist())
    return config,camera_origin


def save_report(path, image, base, **extra):
    report = base|extra|dict(image_sha256=hashlib.sha256(image.read_bytes()).hexdigest())
    path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    return report


def probe(sample, kind, output, provenance, selected_views):
    start = time.perf_counter()
    plan = build(sample,kind)
    plan_ms = (time.perf_counter()-start)*1000
    if plan['roof_status']!='connected-roof':
        raise ValueError('; '.join(plan['omissions']))
    start = time.perf_counter()
    product = mesh(plan)
    mesh_ms = (time.perf_counter()-start)*1000
    checks = verify(plan,product)
    stem = f'roof-{sample["label"]}-{kind}'
    mesh_path = output/(stem+'-mesh.npz')
    np.savez_compressed(mesh_path,**product)
    (output/(stem+'-plan.json')).write_text(json.dumps(plan,default=encode,ensure_ascii=False,indent=2)+'\n')
    base = dict(**provenance,representation='roof-plan',label=sample['label'],roof_kind=kind,roof_method=plan['roof']['method'],
                source_id=sample['id'],source_sha256=sample.get('source_sha256'),source_height_interval_m=[plan['base_z_m'],plan['source_top_m']],
                body_top_m=plan['body_top_m'],roof_rise_m=plan['roof']['rise_m'],roof_facets=len(plan['roof']['facets']),
                storeys=plan['storeys'],rooms=sum(len(row) for row in plan['storey_room_ids']),
                doors=sum(len(row) for row in plan['storey_portals']),openings=len(np.unique(product['parts'][product['materials']==3])),
                triangles=len(product['faces']),raw_mesh_bytes=sum(array.nbytes for array in product.values()),
                generation_ms=dict(plan=plan_ms,mesh=mesh_ms),checks=checks,
                limitations=plan['omissions'],source_coverage='Constructive roof components and generic repeated body; zero completed architectural recipe families')
    images = {}
    for view in selected_views:
        image = output/(stem+'-'+view+'.png')
        config,camera_origin = exterior_scene(sample,plan,mesh_path,image,view)
        rendered = run_render(config,output/(stem+'-'+view+'-scene.json'))
        save_report(output/(stem+'-'+view+'-report.json'),image,base,**rendered,profile=config['profile'],
                    view=view,camera_origin=camera_origin,lod_status='forced complete mesh reference; not an admitted far LOD' if view=='far' else 'constructive complete mesh reference')
        images[view] = image
    spatial = output/(stem+'-spatial.png')
    config,_ = exterior_scene(sample,plan,mesh_path,images['near'],'near')
    colours = {key:config['constructive'][key] for key in ('wall_linear','roof_linear')}
    metadata = render_spatial(mesh_path,spatial,provenance['commit'],plan['footprint'],view='exterior',colours=colours)
    image = output/(stem+'.png')
    figure(sample,plan,product,image,spatial,images['near'])
    report = save_report(output/(stem+'-report.json'),image,base,renderer='Metric Matplotlib / Blender Workbench / Eevee',
                         profile=[1600,1000],view='plan',spatial_reference=metadata)
    print(json.dumps(dict(case=sample['label'],kind=kind,storeys=plan['storeys'],triangles=report['triangles'])),flush=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/roof-plans'))
    parser.add_argument('--cases',nargs='+',default=DEFAULT_CASES)
    parser.add_argument('--kinds',nargs='+',choices=KINDS,default=KINDS)
    parser.add_argument('--views',nargs='+',choices=('near','orbit','far'),default=('near','orbit','far'))
    args = parser.parse_args()
    if 'near' not in args.views:
        parser.error('near view is required for the composite comparison')
    args.output.mkdir(parents=True,exist_ok=True)
    provenance = revision()
    rows = [probe(sample,kind,args.output,provenance,args.views) for sample in cases() if sample['label'] in args.cases for kind in args.kinds]
    (args.output/'roof-summary.json').write_text(json.dumps(dict(**provenance,cases=rows,completed_recipe_families=0,
                                                              scope='Roof/body components; far images are full-detail references, not admitted LODs'),indent=2)+'\n')


if __name__=='__main__':
    main()
