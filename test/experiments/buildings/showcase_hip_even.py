"""Actual default/even roof heights, preserved folds and close spatial comparisons."""

import argparse
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

from building_plan_views import patch
from evidence import revision
from outshine_ref.buildings.modelling import Branch,Shape,cga_to_enu
from outshine_ref.buildings.modelling.faces import plane
from outshine_ref.buildings.modelling.selectors import tagged
from showcase_groups import save_plot
from showcase_roof_shed import spatial
from showcase_skeleton_plan import examples,top_mesh
from verify_modelling_roof_shed import source


def roofs(row):
    polygon=row[1];rise=row[4]
    original=source(polygon)
    return original,original.roof_hip(rise,'byHeight'),original.roof_hip(rise,'byHeight',even=True)


def fold_lines(original,roof):
    plan=roof.roof_info.plans[0];surface=roof.roof_info.surfaces[0]
    if surface is None:return ()
    mesh=original.emit();face=original.geometry.polygons[0];normal,_,_,_=plane(mesh['vertices'],face)
    points=mesh['vertices'][list(face.outer)];edges=np.roll(points,-1,axis=0)-points
    tangent=edges[np.argmax(np.linalg.norm(edges,axis=1))];tangent-=normal*(tangent@normal);tangent/=np.linalg.norm(tangent)
    basis=np.array([tangent,np.cross(normal,tangent)])
    projected=cga_to_enu((plan.positions_m+plan.origin)@basis+mesh['vertices'][face.outer[0]])[:,:2]
    owners={}
    for number,tile in enumerate(surface.facets):
        ids=tile.node_indices
        for a,b in zip(ids,ids[1:]+ids[:1]):owners.setdefault(tuple(sorted((a,b))),[]).append(number)
    return tuple(projected[list(edge)] for edge,tiles in owners.items() if len(tiles)==2
                 and surface.facets[tiles[0]].facet==surface.facets[tiles[1]].facet
                 and not np.allclose(surface.facets[tiles[0]].gradient,surface.facets[tiles[1]].gradient,atol=1e-10,rtol=0))


def plan_view(output,provenance,rows):
    begin=time.perf_counter();fig,axes=plt.subplots(2,3,figsize=(38.4,21.6))
    fig.suptitle('Walmdach · even gleicht verbundene konvexe Grate an',fontsize=34)
    for column,row in enumerate((rows[1],rows[2],rows[4])):
        original,standard,even=roofs(row);rise=row[4]
        for level,(label,roof) in enumerate((('Standard',standard),('even',even))):
            mesh=top_mesh(roof);points=mesh['vertices'];axis=axes[level,column]
            colour=axis.tricontourf(points[:,0],points[:,1],mesh['faces'],points[:,2],levels=np.linspace(0.,rise,257),cmap='viridis')
            patch(axis,row[1],facecolor='none',edgecolor='#17272e',linewidth=3.)
            if level:
                for edge in fold_lines(original,roof):axis.plot(edge[:,0],edge[:,1],color='#de423d',linewidth=2.3)
            fig.colorbar(colour,ax=axis,shrink=.62,label='Höhe über Traufe [m]')
            axis.set(aspect='equal',xlabel='Ost [m]',ylabel='Nord [m]');axis.grid(alpha=.15);axis.tick_params(labelsize=15)
            axis.set_title(row[0]+f'\n{label} · maximale Dachhöhe {points[:,2].max():.3f} m',fontsize=22)
    fig.text(.045,.02,'Echte native Ausgabe. Rot: erhaltene Faltkanten innerhalb nichtplanarer Facetten. byHeight wird vor der Gratmittelung angewandt; even kann die Maximalhöhe senken.',fontsize=22)
    fig.tight_layout(rect=(0,.06,1,.94),h_pad=4.,w_pad=3.)
    return save_plot(fig,output,'current-hip-even-plan','Walmdächer · Standard und horizontale Grate',
                     'Dieselben L-/Trapez-/Wiener Quellgrundrisse und Höhenwerte. Feste Faltflächen vor dem UV-Schnitt; Innenhöfe/Umrisse, UV0–9 und Quellenbesitz erhalten. SDK-Gruppierung noch nicht als vollständiges CGA-Orakel belegt.',
                     provenance,begin,[3840,2160],label='hip-even')


def mass(row,roof):
    _,polygon,base,eave,_,_=row;body=Shape.from_polygon(polygon,base).extrude(eave-base)
    roof=roof.translate('rel','world',0.,eave,0.)
    return [(child,0) for child in body.comp('f',Branch(~tagged('extrude.top')),alignment='noAlign')]+[
        (child,3) for child in roof.comp('f',Branch(tagged('roof.top')),alignment='noAlign')]


def spatial_views(output,provenance,rows):
    reports=[]
    for row,stem in ((rows[2],'current-hip-even-trapezoid'),(rows[4],'current-hip-even-vienna')):
        _,standard,even=roofs(row);spacing=row[1].bounds[2]-row[1].bounds[0]+8.
        shapes=mass(row,standard)+[(shape.translate('rel','world',spacing,0.,0.),colour) for shape,colour in mass(row,even)]
        metadata=row[-1]|dict(nominal_roof_height_m=row[4],default_peak_m=standard.scope.size[1],even_peak_m=even.scope.size[1],
                             maximum_shared_height_adjustment_m=even.roof_info.maximum_shared_height_adjustment_m,
                             ridge_group_sizes=[len(g) for g in even.roof_info.surfaces[0].ridge_groups])
        reports.append(spatial(output,provenance,stem,row[0]+' · Standard und even',shapes,metadata,
                               'Links Standard, rechts even auf demselben Quellkörper. Horizontale Gratgruppen und echte Faltflächen; opake Prüfpalette, noch kein vollständiges Architekturrezept. Überhang/Trim bleiben offen.',
                               label='hip-even',direction=(1.,-1.3,1.8)))
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/hip-even-release'))
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True);provenance=revision();rows=examples()
    reports=[plan_view(args.output,provenance,rows),*spatial_views(args.output,provenance,rows)]
    print(reports)
