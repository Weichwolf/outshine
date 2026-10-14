"""Physical logical-face selection and a verified CSG → component → hip-roof chain."""

import argparse
from dataclasses import replace
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

import numpy as np

from boolean_surface_probe import boolean
from boolean_semantic_overlay import overlay
from boolean_shape_probe import assemble
from evidence import revision
from modelling_scope_probe import engine_mesh
from outshine_ref.buildings.modelling import Branch,Frame
from outshine_ref.buildings.modelling.components import Surface
from outshine_ref.buildings.modelling.selectors import tagged
from showcase_hip_exterior import camera_for
from verify_boolean_attributes import authored
from verify_boolean_shape import verify
from verify_boolean_surfaces import controls,reference


def render_scene(output,stem,title,caption,shapes,source,provenance):
    meshes=[];faces=[];materials=[];offset=0
    for shape in shapes:
        mesh=engine_mesh(shape.emit());meshes.append(mesh['vertices']);faces.append(mesh['faces']+offset)
        materials.append(mesh['materials']+np.uint16(13));offset+=len(mesh['vertices'])
    mesh=dict(vertices=np.concatenate(meshes),faces=np.concatenate(faces),materials=np.concatenate(materials))
    path=output/(stem+'-mesh.npz');np.savez_compressed(path,**mesh)
    image=output/(stem+'.png');view=camera_for([(shape,0) for shape in shapes])
    definitions={str(index+13):dict(material.attributes) for index,material in source.geometry.material_defs}
    config=dict(profile=[1920,1080],mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        eye=view.eye.tolist(),target=view.target.tolist(),fov_deg=view.fov_deg,material_definitions=definitions,
        wall_linear=[.3,.4,.3],roof_linear=[.2,.2,.2],spp=64,seed=17,denoise=True,glass='hidden',output=str(image.resolve()))
    scene=output/(stem+'-scene.json');scene.write_text(json.dumps(config,indent=2)+'\n')
    log=Path(tempfile.gettempdir())/('outshine-buildings-'+stem+'-'+provenance['commit'][:9]+'.log')
    with log.open('w') as stream:
        subprocess.run([sys.executable,str(Path(__file__).with_name('mitsuba_interior.py')),str(scene)],stdout=stream,stderr=subprocess.STDOUT,timeout=600,check=True)
    renderer=json.loads(image.with_suffix('.renderer.json').read_text())
    if renderer['included_source_triangles']!=len(mesh['faces']):raise ValueError('Logical-face preview omitted native triangles')
    record=provenance|dict(representation='showcase-current',page='buildings',view='components',label='semantic-shape',title=title,caption=caption,
        profile=[1920,1080],renderer=renderer,triangles=len(mesh['faces']),image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
        material_definitions=definitions,components=len(shapes),completed_recipe_families=0,lod_status='native LOD unproved')
    name=stem+'-report.json';(output/name).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');return name


def render(output):
    output.mkdir(parents=True,exist_ok=True);provenance=revision();reports=[]
    examples={name:(a,b) for name,a,b in controls()}
    a,b=examples['courtyard'];product=assemble(overlay(boolean(authored(a.shape(),0),authored(b.shape(),1),'union'),method='exact-output'))
    verify(product,reference(a,b,'union'));surface=Surface(product.shape);parts=[]
    for index,child in enumerate(product.shape.comp('f')):
        normal=surface.normals[index];frame=child.scope.object_frame
        moved=Frame(tuple(np.asarray(frame.position)+normal*.8),frame.orientation)
        parts.append(replace(child,scope=replace(child.scope,object_frame=moved)))
    reports.append(render_scene(output,'current-logical-csg-faces','Logische CSG-Flächen · auswählbare Bauteile',
        f'Explosionsansicht der tatsächlich ausgewählten Face-Komponenten: {len(parts)} semantische Flächen einschließlich Hofloch, '
        f'{len(product.overlay.product.mesh["faces"])} native Dreiecke mit ihren Materialien und UV0–9. '
        'Die Darstellung versetzt jede Fläche um 0,8 m entlang ihrer Normale.',
        parts,product.shape,provenance))
    a,b=examples['identical'];product=assemble(overlay(boolean(authored(a.shape(),0),authored(b.shape(),1),'union'),method='exact-output'))
    verify(product,reference(a,b,'union'));top=product.shape.comp('f',Branch(tagged('extrude.top')))[0]
    roof=top.roof_hip(1.5,'byHeight')
    reports.append(render_scene(output,'current-logical-csg-roof','CSG → logische Dachfläche → Walmdach',
        'Aus dem CSG-Körper werden sechs logische Flächen. Die ausgewählte Dachfläche steuert den bestehenden Walmdach-Operator '
        'mit 1,5 m Höhe; alle zehn UV-Kanäle und Materialdefinitionen bleiben vorhanden. Analytisch geprüft: Dachvolumen 56 m³. '
        'Demonstration der Operatorverbindung; vollständige Architekturrezepte bleiben offen.',
        [product.shape,roof],product.shape,provenance))
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/logical-shape-release'))
    print(render(parser.parse_args().output))
