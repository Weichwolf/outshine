"""Show edge sweeps as actual side surfaces, with matching metric ground plans."""

from dataclasses import asdict
import hashlib
import json
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from esri_oracle import CASES
from modelling_components import Branch
from modelling_scope_probe import engine_mesh
from modelling_selectors import tagged
from modelling_shape import Shape
from preview import configuration,run_render
from provider_contract import attributes
from samples import read_site
from showcase_profile import PROFILE
from showcase_scene import fitted_points


COLOURS=[(.11,.35,.48),(.87,.60,.13)]


def swept(source,kind,mode,distance):
    return source.comp(kind,Branch('all','='))[0].extrude(distance,mode)


def controls():
    rectangle=Shape.from_polygon(shapely.box(0,0,8,5),2,tags=('control',))
    cases=[]
    for key,kind,mode in [('normal','fe','face.normal'),('vertex','fe','vertex.normal'),('vertical','e','world.up')]:
        product=swept(rectangle,kind,mode,2.)
        cases.append((key,'Rechteck · '+kind+' → '+mode+' · +2 m',rectangle,product,None))
    concave=Shape.from_polygon(shapely.Polygon([(0,0),(9,0),(9,3),(3,3),(3,8),(0,8)]),2)
    cases.append(('concave','Konkaver Rand · verbundene Vertex-Sweeps',concave,swept(concave,'fe','vertex.normal',1.2),None))
    cube=rectangle.extrude(4.)
    cases.append(('cube','Volumenkanten · Vertex-Sweeps aus zwölf Kanten',cube,swept(cube,'e','vertex.normal',1.2),None))
    _,site,identifier=CASES[1];sample=next(row for row in read_site(site)[0] if row['id']==identifier);data=attributes(sample['properties'])
    body=Shape.from_polygon(sample['polygon'],data['minHeight'],tags=(identifier,)).extrude(data['bodyHeight'],'world.up')
    roof=body.comp('f',Branch(tagged('extrude.top')))[0]
    cases.append(('courtyard-wall','Wien · vertikale Außen-/Hofrandflächen',body,swept(roof,'e','world.up',.9),sample))
    cases.append(('courtyard-rim','Wien · verbundenes Außen-/Hofrandband',body,swept(roof,'fe','vertex.normal',.6),sample))
    return cases,roof


def pair_mesh(source,product):
    vertices=[];faces=[];materials=[];parts=[];offset=0
    for index,shape in enumerate((source,product)):
        mesh=engine_mesh(shape.emit());vertices.append(mesh['vertices']);faces.append(mesh['faces']+offset);offset+=len(mesh['vertices'])
        materials.append(np.full(len(mesh['faces']),12+index,dtype=np.uint16));parts.append(np.full(len(mesh['faces']),index,dtype=np.int32))
    return dict(vertices=np.concatenate(vertices),faces=np.concatenate(faces),materials=np.concatenate(materials),parts=np.concatenate(parts))


def render_control(case,output,provenance,camera):
    key,title,source,product,sample=case;mesh=pair_mesh(source,product);stem='current-edge-sweep-'+key
    path=output/(stem+'-mesh.npz');np.savez_compressed(path,**mesh);image=output/(stem+'.png')
    config,_=configuration([],camera,False,image)
    config.update(inspection_workbench=True,constructive=dict(mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                  wall_linear=[1.,1.,1.],roof_linear=[.155,.179,.196],extra_wall_colours=COLOURS))
    result=run_render(config,output/(stem+'-scene.json'))
    record=provenance|result|dict(representation='showcase-current',page='buildings',view='extrusion',label='modelling-edge-sweep',
        title='Modellierkern · '+title,caption='Blau: Quellfläche/-körper · Gold: tatsächliche Kanten-Sweep-Flächen ohne künstliche Kappen. '
        'Face-Normalen erzeugen getrennte Ecken; Vertexnormalen teilen die Endpunkte. Prüfpalette und Workbench-Studiolicht; keine Architektur-Rezeptfamilie.',
        profile=PROFILE,triangles=len(mesh['faces']),sweep_faces=len(product.geometry.polygons),scope_size_m=product.scope.size,
        sweep=asdict(product.sweep_info),raw_mesh_bytes=sum(a.nbytes for a in mesh.values()),completed_recipe_families=0,
        geometry_scope='source surface plus complete generated edge sweep polygons; no invented thickness or solid volume',lod_status='native LOD unproved',
        measurement_scope='Workbench render of inspection mesh; no native frame timing')
    if sample:record.update(source_id=sample['id'],source_sha256=sample['source_sha256'],source_holes=len(sample['polygon'].interiors))
    name=stem+'-report.json';(output/name).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(dict(page='buildings',control=key,seconds=result['render_seconds'])),flush=True);return name


def plan_figure(cases,roof,output,provenance):
    start=time.perf_counter();fig,axes=plt.subplots(2,2,figsize=(15,10));rectangle=cases[0][2]
    examples=[('Getrennte Normalen · +2 m',rectangle,cases[0][3]),('Verbundene Vertexnormalen · +2 m',rectangle,cases[1][3]),
              ('Verbundene Vertexnormalen · −2 m',rectangle,swept(rectangle,'fe','vertex.normal',-2.)),
              ('Wien · Hofrandband · +0,6 m',roof,cases[-1][3])]
    rectangle_points=np.concatenate([engine_mesh(product.emit())['vertices'][:,:2] for _,_,product in examples[:3]])
    rectangle_low=rectangle_points.min(axis=0)-.8;rectangle_high=rectangle_points.max(axis=0)+.8
    for number,(ax,(title,source,product)) in enumerate(zip(axes.flat,examples,strict=True)):
        for shape,colour in ((source,'#28738a'),(product,'#d29b31')):
            vertices=engine_mesh(shape.emit())['vertices'][:,:2]
            for face in shape.geometry.polygons:
                rings=(face.outer,*face.holes)
                if shape is source:
                    for ring in rings:ax.plot(*np.r_[vertices[list(ring)],vertices[ring[0]][None]].T,color=colour,linewidth=1.5)
                else:
                    ax.fill(*vertices[list(face.outer)].T,color=colour,alpha=.45)
                    ax.plot(*np.r_[vertices[list(face.outer)],vertices[face.outer[0]][None]].T,color=colour,linewidth=.9)
        if number<3:ax.set_xlim(rectangle_low[0],rectangle_high[0]);ax.set_ylim(rectangle_low[1],rectangle_high[1])
        else:ax.margins(.12)
        ax.set_aspect('equal');ax.grid(alpha=.2);ax.set_xlabel('Ost [m]');ax.set_ylabel('Nord [m]');ax.set_title(title)
    title='Kantenextrusion · metrische Grundrisse und gemeinsame Ecken'
    fig.suptitle(title+'\nBlau: Quellringe · Gold: erzeugte Flächen · Normalverschiebung ist kein Miter-Offset',fontsize=13)
    fig.text(.02,.015,'Offene Sweep-Flächen, keine Volumen-/Rezept-/LOD-Abnahme. '+provenance['commit'][:9],fontsize=9)
    fig.subplots_adjust(left=.06,right=.97,bottom=.08,top=.86,wspace=.16,hspace=.28)
    stem='current-edge-sweep-plan';path=output/(stem+'.png');fig.savefig(path,dpi=100);plt.close(fig)
    record=provenance|dict(representation='showcase-current',page='buildings',view='plan',label='modelling-edge-sweep',title=title,
        caption='Gleicher metrischer Maßstab für die drei Rechteckkontrollen. Getrennte Ecken, verbundene Ecken, negative Extrusion und realer Innenhof.',
        renderer='Metric Matplotlib 2D',profile=[1500,1000],render_seconds=time.perf_counter()-start,image_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        completed_recipe_families=0,lod_status='native LOD unproved',measurement_scope='CPU metric plotting/PNG generation; no frame duration')
    name=stem+'-report.json';(output/name).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');return name


def probe(output,provenance):
    cases,roof=controls();rectangle_camera=fitted_points(np.concatenate([pair_mesh(case[2],case[3])['vertices'] for case in cases[:3]]),PROFILE)
    reports=[]
    for number,case in enumerate(cases):
        camera=rectangle_camera if number<3 else fitted_points(pair_mesh(case[2],case[3])['vertices'],PROFILE)
        reports.append(render_control(case,output,provenance,camera))
    reports.append(plan_figure(cases,roof,output,provenance));return reports
