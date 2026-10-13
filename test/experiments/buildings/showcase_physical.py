"""Inspect the actual cached room geometry with physical glass and shared room-light state."""

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

import numpy as np
import shapely

from physical_case import configuration
from showcase_profile import PROFILE
from modelling_metadata import Material
from modelling_shape import Shape
from modelling_scope_probe import engine_mesh


def material_control(output):
    source=Shape.from_polygon(shapely.box(0.,0.,2.,2.)).extrude(3.)
    source=source.with_metadata(material_defs={1:Material('',{'color.r':.75,'color.g':.75,'color.b':.75,'roughness':.8})})
    changed=source.color('#347da4').set_material({'roughness':.2,'metallic':.7})
    cases=[source,changed,changed.reset_material()];positions=[];faces=[];ids=[];definitions={};offset=0
    for index,(shape,identifier) in enumerate(zip(cases,(1,2,4),strict=True)):
        mesh=engine_mesh(shape.t(index*3.,0.,0.).emit());positions.append(mesh['vertices']);faces.append(mesh['faces']+offset)
        ids.append(np.full(len(mesh['faces']),identifier,dtype=np.uint16));offset+=len(mesh['vertices'])
        definitions[str(identifier)]=dict(shape.resolved_materials()[1].attributes)
    path=output/'current-physical-materials-mesh.npz'
    np.savez_compressed(path,vertices=np.concatenate(positions),faces=np.concatenate(faces),materials=np.concatenate(ids))
    config=dict(profile=PROFILE,eye=[11.,-13.,7.],target=[4.,-1.,1.5],fov_deg=50.,wall_linear=[.75,.75,.75],roof_linear=[.2,.2,.2],
        mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),material_definitions=definitions,
        night=False,spp=32,room_lights=[],glass='dielectric',denoise=True,output=str((output/'current-physical-materials.png').resolve()))
    return config,dict(geometry_scope='three complete copies of one owned control prism; actual resolved Shape materials',
        controls=['Geometry material','color + setMaterial','resetMaterial'],resolved_materials=definitions)


def probe(output,provenance):
    reports=[]
    cases=[('rectangle','window',False),('rectangle','window',True),
           ('rectangle','room',False),('rectangle','room',True),
           ('vienna-compact','window',True),('vienna-compact','near',True)]
    for label,view,night in cases+[('materials','controls',False),('rectangle','surface',False)]:
        stem='current-physical-materials' if label=='materials' else 'current-physical-'+label+'-'+view+('-night' if night else '-day')
        image=output/(stem+'.png')
        config,source=material_control(output) if label=='materials' else configuration(label,'near' if view=='surface' else view,night,image,spp=32)
        if view=='surface':config['procedural_panels']=dict(resolution=512)
        config.update(denoise=True,output=str(image.resolve()))
        path=output/(stem+'-scene.json');path.write_text(json.dumps(config,indent=2)+'\n')
        log=Path(tempfile.gettempdir())/('outshine-buildings-'+stem+'-'+provenance['commit'][:9]+'.log')
        with log.open('w') as stream:
            subprocess.run([sys.executable,str(Path(__file__).with_name('mitsuba_interior.py')),str(path)],
                           stdout=stream,stderr=subprocess.STDOUT,check=True,timeout=600)
        renderer=json.loads(image.with_suffix('.renderer.json').read_text())
        record=provenance|source|dict(representation='showcase-current',page='buildings' if view in ('near','controls','surface') else 'interiors',
            view=view,label=label,title='Material: Quelle · Farb-/PBR-Override · Reset' if label=='materials' else ('Wien' if label.startswith('vienna') else 'Kontrollhaus')+' · '+
            dict(window='Fenster und Raum',room='Innenraum',near='Gesamtansicht',surface='Prozedurale Fassadenplatten')[view]+(' · Nacht' if night else ' · Tag'),
            caption='Vollständiges gespeichertes Prüfgebäude mit echten Räumen, Durchgängen und geschlossenem Glas. '
                'Prozedurale Materialien und raumgebundenes Licht; CPU-Pfadverfolgung mit räumlicher Entrauschung. '
                'Generische leere Räume; Einrichtung, Architekturqualität und zeitliche Stabilität offen.',
            profile=PROFILE,renderer=renderer,render_seconds=renderer['render_seconds'],image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
            completed_recipe_families=0,lod_status='physical inspection; native LOD not admitted',
            measurement_scope='two-thread CPU reference, not native frames',log=str(log))
        if label=='materials':record['caption']='Links: Geometriematerial. Mitte: tatsächliche color-/setMaterial-Overrides. Rechts: resetMaterial stellt die Quelle wieder her. Gleiche vollständige Prismen; sRGB einmal in lineares RGB umgerechnet. Physikalische Shaderapproximation, keine Architekturfamilie.'
        if view=='surface':record['caption']='Vollständiges Kontrollhaus mit prozeduralen Farb-, Rauheits- und Normalkarten. Plattenmaß 2,4×1,2 m; Quellfarbe erhalten. Metrische Flächenprojektion mit UV-Nähten, keine CGA-Projektionsgleichheit oder Fernfilterung belegt.'
        name=stem+'-report.json';(output/name).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');reports.append(name)
        print(json.dumps(dict(page=record['page'],title=record['title'],seconds=record['render_seconds'])),flush=True)
    return reports
