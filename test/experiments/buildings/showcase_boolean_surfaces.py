"""Actual CSG massing products and a connected doorway/room inspection shell."""

import argparse
from dataclasses import replace
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from boolean_surface_probe import boolean
from evidence import revision
from outshine_ref.buildings.modelling import Face,Shape
from showcase_groups import save_plot
from showcase_hip_exterior import camera_for
from showcase_roof_shed import spatial
from verify_boolean_surfaces import Prism,check_uv,measure,reference


def as_shape(product):
    mesh=dict(product.mesh);mesh['parts']=np.arange(len(mesh['faces']),dtype=np.int32);faces=[]
    for triangle in mesh['faces']:
        faces.append(Face(tuple(map(int,triangle)),uvs=(tuple(map(tuple,mesh['uv'][triangle])),),
            uv_sets=tuple((c,(tuple(map(tuple,mesh['uv'+str(c)][triangle])),)) for c in range(1,10))))
    result=Shape.from_mesh(mesh,polygons=faces)
    return replace(result,scope=replace(result.scope,object_frame=product.object_frame))


def inspection(product,shift=0.):
    shapes=[]
    for owner,colour in [(0,0),(1,3)]:
        selected=product.operands==owner
        if not np.any(selected):continue
        mesh=dict(product.mesh);mesh['faces']=mesh['faces'][selected]
        shape=Shape.from_mesh(mesh)
        shape=replace(shape,scope=replace(shape.scope,object_frame=product.object_frame))
        shapes.append((shape.translate('rel','world',shift,0.,0.),colour))
    return shapes


def room_reference(room,door):
    box=shapely.box(0,0,12,8);previous=shapely.Polygon();volume=area=0.
    for low,high in [(0.,.2),(.2,3.2),(3.2,6.)]:
        current=box if low==0 else box.difference(room.polygon.union(door.polygon) if low<3.2 else room.polygon)
        volume+=current.area*(high-low)
        area+=current.length*(high-low)+current.symmetric_difference(previous).area;previous=current
    return volume,area+previous.area


def interior_report(output,name):
    path=output/name;report=json.loads(path.read_text());report['page']='interiors'
    path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')


def render(output):
    output.mkdir(parents=True,exist_ok=True);provenance=revision();started=time.perf_counter()
    a=Prism(shapely.box(0,0,12,8),0.,5.)
    b=Prism(shapely.Point(10,4).buffer(3.,quad_segs=12),3.,12.)
    shapes=[];rows=[]
    for n,operation in enumerate(['union','subtract','intersect']):
        product=boolean(a.shape(),b.shape(),operation);actual=measure(product);check_uv(product)
        np.testing.assert_allclose(actual,reference(a,b,operation),atol=1e-7,rtol=1e-9)
        rows.append(dict(operation=operation,volume_m3=actual[0],area_m2=actual[1],triangles=len(product.mesh['faces'])))
        shapes.extend(inspection(product,n*20.))
    reports=[spatial(output,provenance,'current-boolean-three-operations','Baukörper · Vereinigung, Differenz und Schnitt',shapes,
        dict(operations=rows,geometry_scope='actual closed prism CSG; original triangle and UV0–9 provenance'),
        'Von links: Vereinigung, Differenz und Schnitt derselben Körper. Blau zeigt Flächen von A, Gold von B. '
        'Volumen und Oberfläche gegen unabhängige GEOS-Höhenschichten geprüft. Kontrollmassen, kein Architektur-Rezept.',
        label='boolean',camera=camera_for(shapes))]
    room=Prism(shapely.box(2,2,10,6),.2,7.)
    door=Prism(shapely.box(5,-1,7,2.5),.2,3.2)
    cutter=boolean(room.shape(),door.shape(),'union');check_uv(cutter);measure(cutter)
    shell=boolean(Prism(shapely.box(0,0,12,8),0.,6.).shape(),as_shape(cutter),'subtract')
    check_uv(shell);actual=measure(shell)
    np.testing.assert_allclose(actual,room_reference(room,door),atol=1e-7,rtol=1e-9)
    shapes=inspection(shell)
    reports.append(spatial(output,provenance,'current-boolean-doorway','Durchgang · offener Raum als Schnittkontrolle',shapes,
        dict(volume_m3=actual[0],area_m2=actual[1],door_width_m=2.,door_height_m=3.,floor_level_m=.2,
             cutter_union_triangles=len(cutter.mesh['faces']),shell_triangles=len(shell.mesh['faces'])),
        'Ein 2×3-m-Durchgang verbindet außen mit dem offenen Kontrollraum. Türöffnung und Raum besitzen dasselbe '
        'Bodenniveau; negative Hohlraumflächen bleiben erhalten. Tatsächliches geschlossenes Wand-/Bodenvolumen, '
        'keine vollständige Raumplanung, Türenmechanik oder begehbare Rezeptfamilie.',label='boolean',camera=camera_for(shapes)))
    interior_report(output,reports[-1])
    courtyard=shapely.Polygon([(0,0),(16,0),(16,12),(0,12)],[[(3,3),(3,9),(12,9),(12,3)]])
    a=Prism(courtyard,0.,5.);b=Prism(shapely.Point(12,7).buffer(2.8,quad_segs=12),4.,11.)
    product=boolean(a.shape(),b.shape(),'union');actual=measure(product);check_uv(product)
    np.testing.assert_allclose(actual,reference(a,b,'union'),atol=1e-7,rtol=1e-9)
    shapes=inspection(product)
    reports.append(spatial(output,provenance,'current-boolean-courtyard','Hofkörper · überlappender runder Aufsatz',shapes,
        dict(volume_m3=actual[0],area_m2=actual[1],triangles=len(product.mesh['faces']),source_courtyard=True),
        'Der Aufsatz verbindet sich mit dem Hofkörper; innere Überlappungsflächen sind entfernt. Tatsächliche '
        'kontinuierliche Hülle mit Quelle A/B und UV0–9. Prüfpalette, noch keine Fassaden-/Materialkomposition.',
        label='boolean',camera=camera_for(shapes)))
    fig,axes=plt.subplots(1,2,figsize=(38.4,21.6))
    fig.suptitle('CSG-Kontrollraum · vereinigte Schnittkörper und tatsächliches Wandvolumen',fontsize=32)
    for axis,height,title in zip(axes,[1.5,4.5],['Auf Türhöhe 1,5 m','Oberhalb der Tür 4,5 m'],strict=True):
        cut=room.polygon.union(door.polygon) if height<door.high else room.polygon
        wall=shapely.box(0,0,12,8).difference(cut)
        for part in shapely.get_parts(wall):
            xy=np.asarray(part.exterior.coords);axis.fill(*xy.T,color='#226891')
            for ring in part.interiors:
                xy=np.asarray(ring.coords);axis.fill(*xy.T,color='white')
        axis.set(aspect='equal',xlabel='Ost [m]',ylabel='Nord [m]',title=title)
        axis.tick_params(labelsize=18);axis.grid(alpha=.12)
    fig.text(.035,.025,'Raum 8×4 m, Durchgang 2×3 m, Boden 0,2 m. Die zwei CSG-Stufen erhalten native UV0–9; '
             'Abstammung über mehrere Operationen und vollständige Shape-Materialverträge bleiben offen.',fontsize=22)
    fig.tight_layout(rect=(0.,.07,1.,.94))
    reports.append(save_plot(fig,output,'current-boolean-room-plan','CSG-Raum · Grundriss und Türsturz',
        'Unabhängige GEOS-Kontrollschnitte der tatsächlich verwendeten Schnittkörper. Kein Ersatz für die geprüften 3D-Meshes.',
        provenance,started,[3840,2160],label='boolean'))
    interior_report(output,reports[-1])
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/boolean-surface-release'))
    print(render(parser.parse_args().output))
