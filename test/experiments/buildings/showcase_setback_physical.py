"""Physical day and night views of the complete stepped house, with procedural surfaces."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

import numpy as np

from building_vertical import mesh
from evidence import revision
from interior_light_plan import lights
from opening_details import attach,expand,slots
from showcase_setbacks import house


def probe(output,provenance,source=None):
    source=source or output;output.mkdir(parents=True,exist_ok=True);plan=house();reports=[]
    expected=attach(mesh(plan),expand(slots(plan)))
    for view,night in (('near',False),('facade',False),('facade',True)):
        stem='current-setback-physical-'+view+('-night' if night else '-day');image=output/(stem+'.png')
        original=json.loads((source/('current-setback-house-'+view+'-scene.json')).read_text())
        body=original['constructive'];config={key:original[key] for key in ('profile','eye','target','fov_deg','ground_span_m')}
        with np.load(body['mesh']) as stored:
            for key,value in expected.items():np.testing.assert_array_equal(stored[key],value)
        config.update(mesh=body['mesh'],mesh_sha256=body['mesh_sha256'],wall_linear=[.72,.70,.65],roof_linear=[.24,.27,.29],
            procedural_panels=dict(resolution=512),material_overrides={'2':dict(metallic=0.,roughness=.8),'10':dict(metallic=.65,roughness=.28)},
            glass='dielectric',spp=32,denoise=True,night=night,room_lights=lights(plan,'mixed' if night else 'off'),output=str(image.resolve()))
        path=output/(stem+'-scene.json');path.write_text(json.dumps(config,indent=2)+'\n')
        log=Path(tempfile.gettempdir())/('outshine-buildings-'+stem+'-'+provenance['commit'][:9]+'.log')
        with log.open('w') as stream:
            subprocess.run([sys.executable,str(Path(__file__).with_name('mitsuba_interior.py')),str(path)],
                stdout=stream,stderr=subprocess.STDOUT,check=True,timeout=600)
        renderer=json.loads(image.with_suffix('.renderer.json').read_text())
        if renderer['included_source_triangles']!=renderer['source_triangles']:raise ValueError('Physical view omitted source geometry')
        record=provenance|dict(representation='showcase-current',page='buildings',view=view,label='setback-house',
            title='Staffelhaus · '+('Gesamtansicht' if view=='near' else 'Terrasse und Innenraum')+(' · Nacht' if night else ' · Tag'),
            caption='Vollständige Staffelgeschosse, Räume, Rahmen und Terrassen. Prozedurale Farb-/Rauheits-/Normalkarten, physikalisches Glas und geschossbezogenes Raumlicht. '
                'CPU-Referenz mit räumlicher Entrauschung; leere generische Räume, keine fertige Architekturfamilie oder native LOD-Abnahme.',
            profile=config['profile'],renderer=renderer,render_seconds=renderer['render_seconds'],
            image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),completed_recipe_families=0,
            lod_status='physical inspection; native LOD not admitted',source_room_ids=plan['storey_room_ids'],
            geometry_scope='complete stepped house; no visibility truncation',log=str(log))
        name=stem+'-report.json';(output/name).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');reports.append(name)
        print(json.dumps(dict(title=record['title'],seconds=record['render_seconds'])),flush=True)
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=Path('build/experiments/buildings/stepped-facade-release'))
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/stepped-physical-release'))
    args=parser.parse_args();probe(args.output,revision(),args.source)
