"""Physical CSG previews consume captured source material values, not an operand palette."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from boolean_attributes_probe import bind_attributes
from boolean_surface_probe import boolean
from evidence import revision
from modelling_scope_probe import engine_mesh
from showcase_hip_exterior import camera_for
from showcase_boolean_surfaces import inspection
from showcase_groups import save_plot
from boolean_coplanar_audit import audit
from verify_boolean_attributes import authored,check,COLOURS
from verify_boolean_surfaces import Prism,check_uv,measure,reference


def render_product(output,provenance,stem,title,first,second,operation,expected,*,transform=None):
    product=boolean(authored(first,0),authored(second,1),operation)
    if transform is not None:product=transform(product)
    actual=measure(product);np.testing.assert_allclose(actual,expected,atol=1e-7,rtol=1e-9)
    check_uv(product);bound=bind_attributes(product);check(product,bound)
    mesh=engine_mesh(product.mesh);mesh['materials']=bound.materials+np.uint16(13)
    path=output/(stem+'-mesh.npz');np.savez_compressed(path,**mesh)
    view=camera_for(inspection(product));image=output/(stem+'.png')
    definitions={str(n+13):dict(binding.material.attributes) for n,binding in enumerate(bound.material_bindings)}
    config=dict(profile=[1920,1080],mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        eye=view.eye.tolist(),target=view.target.tolist(),fov_deg=view.fov_deg,
        material_definitions=definitions,wall_linear=[.3,.4,.3],roof_linear=[.2,.2,.2],
        spp=64,seed=17,denoise=True,glass='hidden',output=str(image.resolve()))
    scene=output/(stem+'-scene.json');scene.write_text(json.dumps(config,indent=2)+'\n')
    log=Path(tempfile.gettempdir())/('outshine-buildings-'+stem+'-'+provenance['commit'][:9]+'.log')
    with log.open('w') as stream:
        subprocess.run([sys.executable,str(Path(__file__).with_name('mitsuba_interior.py')),str(scene)],
            stdout=stream,stderr=subprocess.STDOUT,timeout=600,check=True)
    renderer=json.loads(image.with_suffix('.renderer.json').read_text())
    if renderer['included_source_triangles']!=len(mesh['faces']):raise ValueError('Physical CSG preview omitted source geometry')
    record=provenance|dict(representation='showcase-current',page='buildings',view='materials',label='boolean-attributes',title=title,
        caption='Tatsächliche Materialwerte der vom Kernel behaltenen Quellflächen: A keramisch grün '
                '(Metallic 0,02/Rauheit 0,61), B metallisch bronze (0,78/0,27). Gleiche Quell-IDs 9 und 3 '
                'bleiben über getrennte Material-/Gruppenbindungen erhalten. Physikalische CPU-Referenz; '
                'koplanare Doppelherkunft/A-Priorität, vollständige Shape-API und Architekturrezepte noch offen.',
        profile=[1920,1080],renderer=renderer,triangles=len(mesh['faces']),image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
        material_definitions=definitions,group_names=bound.group_names,group_origins=bound.group_origins,
        material_origins=[binding.source_materials for binding in bound.material_bindings],
        operation=operation,volume_m3=actual[0],surface_m2=actual[1],completed_recipe_families=0,
        lod_status='native LOD unproved',geometry_scope='closed prism CSG with retained native-owner material bindings',log=str(log))
    name=stem+'-report.json';(output/name).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    return name


def render(output):
    output.mkdir(parents=True,exist_ok=True);provenance=revision();reports=[]
    a=Prism(shapely.box(0,0,12,8),0.,5.)
    b=Prism(shapely.Point(10,4).buffer(3.,quad_segs=12),3.,12.)
    for operation,title in [('union','Baukörper · erhaltene Quellmaterialien'),('subtract','Aussparung · Material der Schnittflächen')]:
        reports.append(render_product(output,provenance,'current-boolean-attributes-'+operation,title,a.shape(),b.shape(),operation,reference(a,b,operation)))
    courtyard=shapely.Polygon([(0,0),(16,0),(16,12),(0,12)],[[(3,3),(3,9),(12,9),(12,3)]])
    a=Prism(courtyard,0.,5.);b=Prism(shapely.Point(12,7).buffer(2.8,quad_segs=12),4.,11.)
    reports.append(render_product(output,provenance,'current-boolean-attributes-courtyard','Hof und Aufsatz · Materialbindungen bleiben getrennt',
        a.shape(),b.shape(),'union',reference(a,b,'union')))
    a=Prism(shapely.box(0,0,12,8),0.,5.);b=Prism(shapely.box(6,3,18,11),0.,5.)
    product=boolean(authored(a.shape(),0),authored(b.shape(),1));proof=audit(product)
    started=time.perf_counter()
    fig,axis=plt.subplots(figsize=(38.4,21.6));fig.suptitle('Koplanare Überlappung · native Dreiecke und fehlende semantische Aufteilung',fontsize=32)
    for number,triangle in enumerate(product.mesh['vertices'][product.mesh['faces']]):
        if not np.all(abs(triangle[:,1]-5.)<1e-9):continue
        xy=triangle[:,[0,2]]*[1.,-1.]
        axis.fill(*xy.T,color=COLOURS[int(product.operands[number])],edgecolor='#273748',linewidth=1.)
    overlap=a.polygon.intersection(b.polygon);xy=np.asarray(overlap.exterior.coords)
    axis.fill(*xy.T,facecolor='none',hatch='///',edgecolor='#101820',linewidth=4.)
    axis.set(aspect='equal',xlabel='Ost [m]',ylabel='Nord [m]');axis.tick_params(labelsize=22);axis.grid(alpha=.12)
    fig.text(.04,.025,f'30 m² Dachüberdeckung; einschließlich Boden 60 m² doppelte Quellstützung. '
             f'{proof["partial"]} teilweise und {proof["full"]} vollständig überdeckte Dreiecke. '
             'Schraffur erfordert eigene bool.A+B-Regionen; eine Schwerpunktmarkierung wäre falsch.',fontsize=22)
    fig.tight_layout(rect=(0.,.07,1.,.94))
    reports.append(save_plot(fig,output,'current-boolean-attributes-coplanar','Koplanare Flächen · offene Doppelherkunft',
        'Tatsächliche native Dachtriangulation, Farben nach behaltenem Operand. Schraffur ist die unabhängig bestimmte '
        '30-m²-Überdeckung; gemischte Dreiecke müssen vor vollständiger Material-/Tagsemantik aufgeteilt werden.',
        provenance,started,[3840,2160],label='boolean-attributes'))
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/boolean-attributes-release'))
    print(render(parser.parse_args().output))
