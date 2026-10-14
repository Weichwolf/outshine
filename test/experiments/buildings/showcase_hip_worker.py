"""Measured process boundary and actual roof products emitted after worker closure."""

import argparse
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from evidence import revision
from outshine_ref.buildings.modelling import HipPlanner,Shape,hip_footprints
from showcase_groups import save_plot
from showcase_hip_exterior import camera_for
from showcase_roof_shed import spatial
from verify_modelling_roof_shed import source


def render(output):
    output.mkdir(parents=True,exist_ok=True);provenance=revision();started=time.monotonic()
    root=Path('build/experiments/buildings');benchmark=json.loads((root/'hip-worker-benchmark.json').read_text())
    proof=json.loads((root/'hip-worker-verification.json').read_text());fig,axes=plt.subplots(1,2,figsize=(38.4,21.6))
    fig.suptitle('Dachplanung · geschützter Desktop-Prozess und wiederverwendbare Pläne',fontsize=32)
    ids=list(dict.fromkeys(r['id'] for r in benchmark['results']));x=np.arange(len(ids))
    for method,offset,colour,title in [('direct',-.2,'#226891','Direkt, kooperativ'),('isolated',.2,'#d29228','Prozess, externe Deadline')]:
        rows={r['id']:r for r in benchmark['results'] if r['execution']==method}
        axes[0].bar(x+offset,[np.median(rows[i]['samples_ms']) for i in ids],width=.4,color=colour,label=title)
    axes[0].set(xticks=x,xticklabels=[i.split(':')[0][0]+str(n+1) for n,i in enumerate(ids)],ylabel='Planung pro Quelle [ms]',title='Gleiche 48 Quellen und Geometrieanforderungen · fünf warme Proben')
    axes[0].tick_params(axis='x',labelsize=11,rotation=90);axes[0].tick_params(axis='y',labelsize=18);axes[0].legend(fontsize=18);axes[0].grid(axis='y',alpha=.15)
    controls=proof['controls'];values=[controls['timeout']['elapsed_ms'],controls['cancel']['elapsed_ms'],benchmark['startup_ms']]
    axes[1].barh(['Native Blockade → beendet','Abbruch → beendet','Neuer Prozess → bereit'],values,color=['#c63343','#368d77','#647386'])
    for n,value in enumerate(values):axes[1].text(value+4,n,f'{value:.1f} ms',va='center',fontsize=23)
    axes[1].axvline(200.,color='#c63343',linestyle='--',label='200-ms-Anfragelimit')
    axes[1].set(xlabel='Gemessene Dauer [ms]',title='Blockierte libc.pause-Funktion, SIGKILL und vollständiges Reaping',xlim=(0,max(values)*1.35))
    axes[1].tick_params(labelsize=19);axes[1].legend(fontsize=18);axes[1].grid(axis='x',alpha=.15)
    fig.text(.035,.025,'Desktop-Referenz: eine Anfrage je Worker, begrenzte Nachrichten, Verbindung nach Abbruch verwerfen. '
             'Prozessstart und Mesh-Ausgabe haben eigene Budgets; kein iOS-/A18-/Frame-Nachweis.',fontsize=22)
    fig.tight_layout(rect=(0.,.07,1.,.94),w_pad=5.)
    reports=[save_plot(fig,output,'current-hip-worker-cost','Dachplanung · messbarer Prozessabbruch',
        'Alle 48 unveränderten Quellen werden vor dem Kostenvergleich geometrisch geprüft. Die native Blockade ist ein '
        'tatsächlicher libc.pause-Aufruf; der bekannte CGAL-Gegenfall hängt auf diesem Linux-Host nicht. Startup separat.',
        provenance,started,[3840,2160],label='hip-worker')]
    hole=[(6.3816405375158,7.138778040313788),(5.117961890258449,5.588579254200913),
          (8.2183594624842,3.061221959686211),(9.48203810974155,4.611420745799086)]
    polygon=shapely.Polygon([(0,0),(16.7,0),(16.7,12.3),(0,12.3)],[hole]);authored=source(polygon)
    with HipPlanner() as planner:plans=tuple(planner.prepare(p) for p in hip_footprints(authored));worker_pid=planner.pid
    shapes=[]
    for n,(height,even) in enumerate([(2.,False),(3.,True)]):
        roof=authored.roof_hip(height,'byHeight',even=even,prepared_plans=plans)
        body=Shape.from_polygon(polygon).extrude(5.)
        shapes.extend([(body.translate('rel','world',n*23.,0.,0.),0),(roof.translate('rel','world',n*23.,5.,0.),3)])
    reports.append(spatial(output,provenance,'current-hip-worker-products','Ein gespeicherter Plan · zwei geschlossene Dachprodukte',shapes,
        dict(worker_pid=worker_pid,worker_closed_before_emission=True,prepared_plans=len(plans),inspection_body_height_m=5.,
             roof_parameters=[dict(height_m=2.,even=False),dict(height_m=3.,even=True)]),
        'Links Standarddach 2 m, rechts even-Dach 3 m auf derselben gedrehten Hofdomäne. Die tatsächlichen Meshes entstehen '
        'aus demselben unveränderlichen Plan nach Schließen des Prozesses, ohne erneuten Skelettsolver. Prüfpalette, keine '
        'fertige Architektur-Rezeptfamilie.',label='hip-worker',camera=camera_for(shapes)))
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/hip-worker-release'))
    print(render(parser.parse_args().output))
