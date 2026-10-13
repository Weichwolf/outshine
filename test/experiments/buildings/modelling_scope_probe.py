"""Measure deferred versus eager scope transforms and show their geometry on the cached house control."""

import hashlib
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

from evidence import revision
from outshine_ref.buildings.modelling import Relative,cga_to_enu,enu_to_cga
from modelling_shape import Shape
from preview import configuration,run_render
from showcase_scene import fitted_points


def axis_map(mesh,convert):
    return mesh|{name:convert(mesh[name]) for name in ('vertices','normals') if name in mesh}


def cga_mesh(mesh):return axis_map(mesh,enu_to_cga)


def engine_mesh(mesh):return axis_map(mesh,cga_to_enu)


def programme(scope):
    scope=scope.r(12.,25.,-9.);yield scope
    scope=scope.t(Relative(.2),2.,Relative(-.1));yield scope
    scope=scope.s(Relative(1.2),Relative(.8),Relative(.9));yield scope
    scope=scope.r(-5.,11.,7.,'scopeCenter');yield scope
    yield scope.translate('rel','world',3.,-2.,4.)


def deferred(shape):
    final=shape.scope
    for current in programme(shape.scope):final=current
    return Shape(shape.geometry,final).emit()['vertices']


def eager(shape,vertices):
    current=shape.scope;result=vertices
    for successor in programme(current):
        old=current.matrix;new=successor.matrix
        change=new@np.linalg.inv(old)
        result=result@change[:3,:3].T+change[:3,3]
        current=successor
    return result


def benchmark(source,city):
    rows=[]
    paths=[source/f'opening-{label}-mesh.npz' for label in ('rectangle','vienna-compact','tokyo-compact')]
    paths.append(city/'tokyo-city-mesh.npz')
    for path in paths:
        mesh=cga_mesh(dict(np.load(path)));shape=Shape.from_mesh(mesh)
        a=deferred(shape);b=eager(shape,mesh['vertices'])
        error=float(abs(a-b).max(initial=0.))
        if error>1e-8:raise AssertionError('Deferred/eager transform alternatives differ on the cached geometry')
        for name,call in (('deferred',lambda:deferred(shape)),('eager',lambda:eager(shape,mesh['vertices']))):
            call();times=[]
            for _ in range(21):
                start=time.perf_counter_ns();call();times.append((time.perf_counter_ns()-start)/1e6)
            rows.append(dict(asset=path.name,method=name,vertices=len(mesh['vertices']),triangles=len(mesh['faces']),
                             p50_ms=float(np.percentile(times,50)),p95_ms=float(np.percentile(times,95)),
                             geometry_reused=True,maximum_coordinate_difference_m=error,
                             work='five identical scope operations; emitted positions only, no normal/UV generation or native frame time'))
    return rows


def controls(shape):
    rotated=shape.r(0.,45.,0.,'scopeCenter')
    return [('original','Unverändert',shape),('origin','Rotation am Scope-Ursprung',shape.r(0.,45.,0.)),
            ('centre','Rotation am Scope-Zentrum',rotated),
            ('translation','Verschiebung entlang gedrehter Achsen',rotated.t(Relative(.4),0.,Relative(.2))),
            ('mirror','Negative Scope-Größe',shape.s(Relative(-1.),Relative(1.),Relative(.7)))]


def plan_figure(cases,output,provenance):
    start=time.perf_counter()
    fig,ax=plt.subplots(figsize=(12,8));colours=plt.get_cmap('tab10').colors
    for index,(_,name,shape) in enumerate(cases):
        scope=shape.scope;corners=np.array([[0.,0.,0.],[1.,0.,0.],[1.,0.,1.],[0.,0.,1.],[0.,0.,0.]])
        points=corners@scope.matrix[:3,:3].T+scope.origin;xy=points[:,[0,2]]*[1.,-1.]
        ax.plot(*xy.T,color=colours[index],label=name)
        origin=scope.origin[[0,2]]*[1.,-1.];centre=scope.centre[[0,2]]*[1.,-1.]
        ax.scatter(*origin,color=colours[index],marker='s');ax.scatter(*centre,color=colours[index],marker='x')
    ax.set_aspect('equal');ax.set_xlabel('Ost [m]');ax.set_ylabel('Nord [m]');ax.grid(alpha=.2);ax.legend()
    ax.set_title('Derselbe Hauskontrollkörper · Scope-Operationen\nQuadrat: Scope-Ursprung · Kreuz: Mittelpunkt')
    fig.text(.02,.015,'CGA-Achsen (Ost, Auf, Süd) → Outshine (Ost, Nord, Auf). Kontrolltransformationen, keine neuen OSM-Rezepte. '+provenance['commit'][:9],fontsize=8)
    fig.tight_layout(rect=(0,.045,1,1));fig.savefig(output,dpi=100);plt.close(fig)
    return time.perf_counter()-start


def render_controls(source,output,provenance):
    mesh=dict(np.load(source/'opening-rectangle-mesh.npz'));shape=Shape.from_mesh(cga_mesh(mesh))
    cases=controls(shape);all_points=np.concatenate([engine_mesh(body.emit())['vertices'] for _,_,body in cases])
    camera=fitted_points(all_points,[1280,720])
    reports=[]
    base=dict(**provenance,representation='showcase-current',page='buildings',label='modelling-scope',view='scope',
              triangles=len(mesh['faces']),raw_mesh_bytes=sum(array.nbytes for array in mesh.values()),storeys=1,rooms=4,doors=4,
              completed_recipe_families=0,geometry_scope='cached synthetic house control; transformed scopes, no provider reconstruction',
              lod_status='complete control geometry; native LOD unproved')
    for key,name,body in cases:
        stem='current-scope-'+key;product=engine_mesh(body.emit());path=output/(stem+'-mesh.npz')
        np.savez_compressed(path,**product);image=output/(stem+'.png')
        config,_=configuration([],camera,False,image)
        config.update(inspection_workbench=True,constructive=dict(mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                                                 wall_linear=[1.,1.,1.],roof_linear=[.155,.179,.196]))
        result=run_render(config,output/(stem+'-scene.json'))
        report=base|result|dict(title='Modellierkern · '+name,caption='Python-Scope-Operationen am selben vollständigen Hauskontrollmesh. '
                               'Einheitliche Kamera und Workbench-Studiolicht; Transformationen sind Funktionskontrollen.',
                               profile=[1280,720],scope_position_m=body.scope.position,scope_orientation_deg=body.scope.orientation,scope_size_m=body.scope.size)
        name=stem+'-report.json';(output/name).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');reports.append(name)
        print(json.dumps(dict(page='buildings',control=key,seconds=result['render_seconds'])),flush=True)
    image=output/'current-scope-plan.png';seconds=plan_figure(cases,image,provenance)
    report=base|dict(title='Modellierkern · Scope im Grundriss',caption='Ursprung und Mittelpunkt derselben fünf Kontrolltransformationen; metrische 2D-Projektion der Scope-Boxen.',
                     renderer='Metric Matplotlib',profile=[1200,800],render_seconds=seconds,image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
                     view='plan',measurement_scope='2D scope diagram; CPU plotting/PNG generation, not Blender or frame duration')
    name='current-scope-plan-report.json';(output/name).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');reports.append(name)
    return reports


def probe(source,city,output,provenance):
    rows=benchmark(source,city)
    (output/'scope-benchmark.json').write_text(json.dumps(provenance|dict(measurements=rows),ensure_ascii=False,indent=2)+'\n')
    return render_controls(source,output,provenance)
