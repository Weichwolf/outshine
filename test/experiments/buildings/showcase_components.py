"""Show selected faces, local component movement, courtyard groups and facade frames."""

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
from modelling_faces import Face
from modelling_scope_probe import engine_mesh
from modelling_selectors import tagged
from modelling_shape import Shape
from preview import configuration,run_render
from provider_contract import attributes
from samples import read_site
from showcase_scene import fitted_points


PALETTE=[(.13,.48,.72),(.75,.23,.14),(.85,.65,.16),(.17,.56,.34),(.51,.29,.65),(.50,.48,.41)]
DIRECTIONS=('front','back','left','right','top','bottom')


def coloured(shapes,per_component=False):
    vertices=[];faces=[];materials=[];parts=[];offset=0
    key=lambda shape:(shape.comp_info.sel,shape.comp_info.index if per_component else 0)
    colour={name:index for index,name in enumerate(dict.fromkeys(key(shape) for shape in shapes))}
    for number,shape in enumerate(shapes):
        mesh=engine_mesh(shape.emit());vertices.append(mesh['vertices']);faces.append(mesh['faces']+offset);offset+=len(mesh['vertices'])
        materials.append(np.full(len(mesh['faces']),12+colour[key(shape)],dtype=np.uint16))
        parts.append(np.full(len(mesh['faces']),number,dtype=np.int32))
    return dict(vertices=np.concatenate(vertices),faces=np.concatenate(faces),materials=np.concatenate(materials),parts=np.concatenate(parts)),len(colour)


def controls():
    source=Shape.from_polygon(shapely.box(0,0,8,5),4,tags=('control-body',)).extrude(4)
    branches=tuple(Branch(name) for name in DIRECTIONS)
    aligned=source.comp('f',*branches);fixed=source.comp('f',*branches,alignment='noAlign')
    cases=[('cube','Sechs Richtungen · Weltlage erhalten',aligned),
           ('zup','zUp · t(0, 0, 1,2) je Flächenscope',tuple(shape.t(0,0,1.2) for shape in aligned)),
           ('noalign','noAlign · dieselbe lokale Verschiebung',tuple(shape.t(0,0,1.2) for shape in fixed))]
    wall=source.comp('f',Branch('side'));panels=tuple(shape.t(0,0,.35).extrude(.3) for shape in wall)
    cases.append(('panels','Flächenscope → 0,3 m Extrusion',panels))
    vertices=np.array([[0,4,0],[4,4,0],[4,6,4],[0,6,4],[4,4,8],[0,4,8]],dtype=float)
    roof=Shape.from_faces(vertices,[Face((3,2,1,0)),Face((5,4,2,3))]).extrude(3,'world.up')
    cases.append(('inclination','Dachvolumen · Neigungsauswahl',roof.comp('f',*(Branch(name) for name in ('horizontal','aslant','vertical','nutant')))))
    return aligned,cases


def render_control(output,provenance,key,title,shapes,camera,source=None,per_component=False):
    mesh,colours=coloured(shapes,per_component);stem='current-components-'+key;path=output/(stem+'-mesh.npz');np.savez_compressed(path,**mesh)
    image=output/(stem+'.png');configuration_data,_=configuration([],camera,False,image)
    configuration_data.update(inspection_workbench=True,constructive=dict(mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        wall_linear=[1.,1.,1.],roof_linear=[.155,.179,.196],extra_wall_colours=PALETTE[:colours]))
    result=run_render(configuration_data,output/(stem+'-scene.json'))
    groups=[dict(selector=shape.comp_info.sel,index=shape.comp_info.index,total=shape.comp_info.total,
                 source_indices=shape.comp_info.source_indices,scope_size_m=shape.scope.size) for shape in shapes]
    caption='Farben zeigen Auswahlgruppen. Flächenringe, Materialien und Corner-UVs sind in den Komponenten erhalten; Prüfpalette ersetzt nur die Bildfarben. '
    caption+='OSM-Hofgrundriss und gelieferte Höhe.' if source else 'Synthetische Funktionskontrolle; Workbench-Studiolicht, keine Architektur-Rezeptfamilie.'
    record=provenance|result|dict(representation='showcase-current',page='buildings',view='components',label='modelling-components',
        title='Modellierkern · '+title,caption=caption,profile=[1280,720],triangles=len(mesh['faces']),logical_faces=sum(len(shape.geometry.polygons) for shape in shapes),
        components=len(shapes),component_groups=groups,source_holes=len(source['polygon'].interiors) if source else 0,
        raw_mesh_bytes=sum(array.nbytes for array in mesh.values()),completed_recipe_families=0,
        geometry_scope='selected face components and explicit local operations; no new architecture recipe',lod_status='native LOD unproved',
        measurement_scope='coloured inspection payload: vertices, triangle indices, material and component ids; complete component data measured separately')
    if source:record.update(source_id=source['id'],source_sha256=source['source_sha256'])
    name=stem+'-report.json';(output/name).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(dict(page='buildings',components=key,seconds=result['render_seconds'])),flush=True)
    return name


def face_plan(shapes,path,provenance):
    began=time.perf_counter();fig,axes=plt.subplots(2,3,figsize=(15,9))
    for number,(ax,shape) in enumerate(zip(axes.flat,shapes,strict=True)):
        mesh=shape.emit();points=(mesh['vertices']-shape.scope.origin)@shape.scope.axes
        face=shape.geometry.polygons[0];xy=points[list(face.outer),:2]
        ax.fill(*xy.T,color=PALETTE[number],alpha=.24);ax.plot(*np.r_[xy,xy[:1]].T,color=PALETTE[number])
        for triangle in mesh['faces']:ax.plot(*np.r_[points[triangle,:2],points[triangle[:1],:2]].T,color='#89969a',linewidth=.6)
        pivot=(shape.scope.pivot_origin-shape.scope.origin)@shape.scope.axes;ax.scatter(*pivot[:2],marker='s',color='#28313c',zorder=5)
        ax.set_title(shape.comp_info.sel+' · '+f'{shape.scope.size[0]:g} × {shape.scope.size[1]:g} m · z = 0')
        ax.set_aspect('equal');ax.grid(alpha=.2);ax.set_xlabel('Lokales x [m]');ax.set_ylabel('Lokales y [m]')
    fig.suptitle('Ein Volumen → sechs lokale Flächenscopes\nVorhandene Dreiecke übernommen · Quadrat: Pivot am ersten Vertex · Normalen entlang lokalem z',fontsize=13)
    fig.text(.02,.015,'CGA (Ost, Auf, Süd) → metrische Komponentenebenen. Weltlage, UVs und ursprüngliche Ringe erhalten. '+provenance['commit'][:9],fontsize=9)
    fig.tight_layout(rect=(0,.035,1,.93));fig.subplots_adjust(hspace=.55,top=.84,bottom=.095)
    fig.savefig(path,dpi=100);plt.close(fig)
    return time.perf_counter()-began


def probe(output,provenance):
    aligned,cases=controls();camera=fitted_points(np.concatenate([coloured(shapes)[0]['vertices'] for _,_,shapes in cases]),[1280,720])
    reports=[render_control(output,provenance,key,title,shapes,camera) for key,title,shapes in cases]
    _,site,identifier=CASES[1];sample=next(row for row in read_site(site)[0] if row['id']==identifier);data=attributes(sample['properties'])
    body=Shape.from_polygon(sample['polygon'],data['minHeight'],tags=(identifier,)).extrude(data['bodyHeight'],'world.up')
    groups=body.comp('f',Branch(tagged('extrude.side.outer'),'='),Branch(tagged('extrude.side.inner'),'='),Branch(tagged('extrude.top'),'='),Branch('all','='))
    view=fitted_points(engine_mesh(body.emit())['vertices'],[1280,720])
    reports.append(render_control(output,provenance,'courtyard','Wien · Außenwand, Hofwand und Dach',groups,view,sample))
    walls=body.comp('f',Branch(tagged('extrude.side'),'=:'))
    reports.append(render_control(output,provenance,'courtyard-connected','Wien · zwei verbundene Wandgruppen',walls,view,sample,True))
    stem='current-components-planes';image=output/(stem+'.png');seconds=face_plan(aligned,image,provenance)
    report=provenance|dict(representation='showcase-current',page='buildings',view='plan',label='modelling-components',title='Modellierkern · Flächen im lokalen 2D-Scope',
        caption='Dieselben sechs Flächen des Kontrollvolumens in ihren eigenen metrischen Ebenen. Vorhandene Dreiecke, lokale Maße und Pivot am ersten Flächenvertex.',
        renderer='Metric Matplotlib',profile=[1500,900],render_seconds=seconds,image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
        completed_recipe_families=0,lod_status='component diagram; native LOD unproved',measurement_scope='CPU plotting/PNG generation; no frame duration')
    name=stem+'-report.json';(output/name).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');reports.append(name)
    return reports
