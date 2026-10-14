"""A verified coplanar control, explicitly separate from rejected general overlays."""

import argparse
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from boolean_semantic_overlay import overlay
from boolean_semantic_attributes import attributes,cut_edges
from evidence import revision
from showcase_boolean_attributes import render_product
from showcase_groups import save_plot
from verify_boolean_semantic_overlay import support_coverage,check
from verify_boolean_surfaces import Prism,reference,measure,check_uv


def render(output):
    output.mkdir(parents=True,exist_ok=True);provenance=revision();reports=[];captured=[]
    a=Prism(shapely.box(0,0,12,8),0.,5.);b=Prism(shapely.box(6,3,18,11),0.,5.)
    def process(native):
        result=overlay(native,method='triangle');check_uv(result.product);bound=attributes(result);check(result,bound)
        dual=support_coverage(result);np.testing.assert_allclose(dual,60.,atol=1e-7)
        captured.append((result,bound,cut_edges(result)));return result.product
    report=render_product(output,provenance,'current-semantic-coplanar-material','Koplanarer Kontrollfall · A-Material im Überlappungsbereich',
        a.shape(),b.shape(),'union',reference(a,b,'union'),transform=process)
    reports.append(report);path=output/report;record=json.loads(path.read_text());result,bound,edges=captured[0]
    record.update(caption='Geprüfter Kontrollfall: Grün ist Operand A, Bronze B; die Überdeckung erhält Material A '
        'und bool.A+B. Ganze Regionen statt Schwerpunktzuweisung; Schließung, Oberfläche, Volumen und UV0–9 geprüft. '
        'Der allgemeine Versuch ist noch nicht freigegeben: der 2-mm-Schnitt scheitert an der Topologie.',
        label='semantic-overlay',control_only=True,full_suite_admitted=False,dual_area_m2=60.,cut_edges=len(edges),
        scope='verified coplanar control only; general semantic overlay remains rejected')
    path.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    started=time.perf_counter();fig,axis=plt.subplots(figsize=(38.4,21.6))
    fig.suptitle('Koplanare Regionen · beide Quellen, Material A und tatsächliche Schnittkanten',fontsize=32)
    p=result.product
    for number,triangle in enumerate(p.mesh['vertices'][p.mesh['faces']]):
        if not np.all(abs(triangle[:,1]-5.)<1e-9):continue
        xy=triangle[:,[0,2]]*[1.,-1.];dual=len(result.supports[number])==2
        axis.fill(*xy.T,color='#74a090' if p.operands[number]==0 else '#b58c4a',
                  edgecolor='#344454',linewidth=.7,hatch='///' if dual else None)
    coords={int(root):point for root,point in zip(p.roots,p.mesh['vertices'],strict=True)}
    for edge in edges:
        xyz=np.asarray([coords[int(root)] for root in edge])
        if np.all(abs(xyz[:,1]-5.)<1e-9):axis.plot(xyz[:,0],-xyz[:,2],color='#a8203d',linewidth=3.)
    axis.set(aspect='equal',xlabel='Ost [m]',ylabel='Nord [m]');axis.tick_params(labelsize=22);axis.grid(alpha=.12)
    fig.text(.035,.025,'Schraffur: 30 m² Dachfläche mit bool.A+B; Grün: Materialpriorität A. Rot: abgeleitete bool.cut-Kanten. '
        'Kontrollfall bestanden; allgemeiner 2-mm-Gegenfall bleibt rot.',fontsize=22)
    fig.tight_layout(rect=(0.,.07,1.,.94))
    reports.append(save_plot(fig,output,'current-semantic-coplanar-plan','Koplanarer Kontrollfall · wirklich geteilte Regionen',
        'Tatsächliche erzeugte Dachtriangulation nach Source-Overlay. Doppelte Herkunft und Schnittkanten; keine allgemeine '
        'Shape-/SDK-/Framefreigabe aus diesem einzelnen Kontrollbild.',provenance,started,[3840,2160],label='semantic-overlay'))
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/semantic-overlay-release'))
    print(render(parser.parse_args().output))
