"""Attach metrical opening components to cached constructive building plans and render the result."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely

from building_plan_probe import render_spatial
from demand import View
from evidence import revision
from opening_details import attach,expand,slots
from opening_modules import template
from opening_views import figure
from preview import configuration,run_render
from verify_openings import decoded,verify_body_openings,verify_slots


def scene(source, output, mesh_path, view, openings):
    reference=json.loads(source.read_text())
    if view in ('near','orbit','far'):
        config=reference
    else:
        window=next(row for row in openings if row['kind']==('entrance' if view=='entrance' else 'window')
                    and row.get('outer_facade',False))
        tangent=np.asarray(window['tangent']);outward=np.array([tangent[1],-tangent[0]])
        centre=np.array([*window['centre'],window['bottom_z_m']+window['height_m']/2])
        eye=centre+np.array([*(outward*3.6-tangent*.55),.12])
        camera=View(eye,centre,width=reference['profile'][0],height=reference['profile'][1])
        config,_=configuration([],camera,True,output)
        config['environment_centre']=reference.get('environment_centre',reference['target'])
        config['ground_span_m']=reference['ground_span_m']
    config.update(output=str(output.resolve()),products=[],tiles={},solar_tile=None)
    config['constructive']=reference['constructive']|dict(mesh=str(mesh_path.resolve()),
                     mesh_sha256=hashlib.sha256(mesh_path.read_bytes()).hexdigest(),glass_alpha=.08)
    return config


def probe(label, source, output, provenance):
    original=source/f'roof-{label}-flat'
    plan=json.loads(original.with_name(original.name+'-plan.json').read_text(),object_hook=decoded)
    body=dict(np.load(original.with_name(original.name+'-mesh.npz')))
    start=time.perf_counter();openings=slots(plan);detail=expand(openings);ms=(time.perf_counter()-start)*1000
    checks=verify_slots(plan,openings,detail);verify_body_openings(openings,body)
    for row in openings:
        row['outer_facade']=plan['footprint'].exterior.distance(shapely.Point(row['centre']))<1e-6
    product=attach(body,detail);stem='opening-'+label;mesh_path=output/(stem+'-mesh.npz')
    np.savez_compressed(mesh_path,**product)
    (output/(stem+'-slots.json')).write_text(json.dumps(openings,ensure_ascii=False,indent=2)+'\n')
    base=dict(**provenance,representation='opening-components',label=label,source_id=plan['id'],checks=checks,
              openings=len(openings),rooms=len({identifier for row in openings for identifier in row['room_ids']}),
              doors=sum(row['kind']=='entrance' for row in openings),storeys=plan['storeys'],triangles=len(product['faces']),
              component_triangles=len(detail['faces']),raw_mesh_bytes=sum(a.nbytes for a in product.values()),
              source_plan_sha256=hashlib.sha256(original.with_name(original.name+'-plan.json').read_bytes()).hexdigest(),
              source_mesh_sha256=hashlib.sha256(original.with_name(original.name+'-mesh.npz').read_bytes()).hexdigest(),
              generation_ms=dict(opening_slots_and_expansion=ms),completed_recipe_families=0,
              limitations=['generic empty rooms; thin alpha glass approximation; native LOD and night light unproved',
                           'forced complete detail references; visibility/shadow demand not admitted'],
              source_coverage='Opening components attached to existing cached room/portal plan')
    views={}
    for view in ('near','orbit','far','window','entrance'):
        image=output/(stem+'-'+view+'.png')
        reference=source/(original.name+'-'+(view if view in ('near','orbit','far') else 'near')+'-scene.json')
        config=scene(reference,image,mesh_path,view,openings)
        rendered=run_render(config,output/(stem+'-'+view+'-scene.json'))
        report=base|rendered|dict(profile=config['profile'],view=view,
                    image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),glass_alpha=.08,
                    camera_origin='same cached roof/Esri camera and lighting' if view in ('near','orbit','far') else 'opening close view; same environment centre/light')
        (output/(stem+'-'+view+'-report.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
        views[view]=image
    module=output/(stem+'-window-module.npz');np.savez_compressed(module,**template())
    spatial=output/(stem+'-module-spatial.png')
    metadata=render_spatial(module,spatial,provenance['commit'],shapely.box(-.7,-.2,.7,.2),view='exterior',camera_tangent=[1.,0.])
    image=output/(stem+'.png');figure(label,openings,image,spatial,views['near'],views['window'])
    report=base|dict(profile=[1600,1200],view='plan',renderer='Metric Matplotlib / Blender Workbench / Eevee',
                    image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),spatial_reference=metadata)
    (output/(stem+'-report.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(dict(label=label,openings=len(openings),triangles=len(product['faces']))),flush=True)
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=Path('build/experiments/buildings/roof-plans'))
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/opening-plans'))
    parser.add_argument('--cases',nargs='+',default=['rectangle','vienna-compact','vienna-courtyard','tokyo-compact'])
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True);provenance=revision()
    rows=[probe(label,args.source,args.output,provenance) for label in args.cases]
    (args.output/'opening-summary.json').write_text(json.dumps(dict(**provenance,cases=rows),ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':
    main()
