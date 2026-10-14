"""Actual thin-cut triangulation and full-suite failures; no general admission."""

import argparse
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

from boolean_surface_probe import boolean
from boolean_semantic_overlay import overlay
from boolean_semantic_attributes import attributes,cut_edges
from evidence import revision
from showcase_groups import save_plot
from verify_boolean_attributes import authored
from verify_boolean_semantic_overlay import support_coverage,check
from verify_boolean_surfaces import controls,measure,reference,check_uv


def render(output,reports):
    output.mkdir(parents=True,exist_ok=True);provenance=revision();start=time.perf_counter()
    rows=[json.loads(path.read_text()) for path in reports]
    for row in rows:
        if row['commit']!=provenance['commit'] or row['experiment_tree_dirty']:
            raise ValueError('Exact comparison requires clean reports from current commit')
        if row['cases']!=183:raise ValueError('Complete comparison requires all 183 products')
    name,a,b=next(row for row in controls() if row[0]=='thin-passage')
    result=overlay(boolean(authored(a.shape(),0),authored(b.shape(),1),'intersect'),method='exact-rounded')
    np.testing.assert_allclose(measure(result.product),reference(a,b,'intersect'),atol=1e-7,rtol=1e-9)
    check_uv(result.product);support_coverage(result);check(result,attributes(result));cut_edges(result)
    fig,(plan,table)=plt.subplots(1,2,figsize=(32,18),gridspec_kw={'width_ratios':[1,1.25]})
    p=result.product
    for triangle in p.mesh['vertices'][p.mesh['faces']]:
        if np.all(triangle[:,1]==5.):
            xy=np.c_[(triangle[:,0]-6.)*1000.,-triangle[:,2]]
            plan.fill(*xy.T,color='#74a090',edgecolor='#344454',linewidth=1.)
    plan.set(xlabel='Ost relativ zur Mitte [mm]',ylabel='Nord [m]',title='2-mm-Schnitt: tatsächliche Dachtriangulation')
    plan.tick_params(labelsize=17);plan.grid(alpha=.2)
    plan.text(.02,-.12,'Querachse in mm, Längsachse in m; unterschiedliche Maßstäbe.\n'
              'Dieser Schnitt besteht Volumen, Oberfläche, Topologie, UVs und Attribute.',transform=plan.transAxes,fontsize=16)
    table.axis('off');lines=['Gleiche vollständige Prüfung: 183 Produkte','']
    for row in rows:
        lines.append(f"{row['method']}: {row['passed']}/{row['cases']} bestanden")
        for case in row['results']:
            if case['status']!='pass':lines.append(f"  {case['id']} / {case['operation']}: {case['reason']}")
        times=[case['overlay_ms'] for case in row['results'] if 'overlay_ms' in case]
        lines.append('  Host-Einzelaufrufe p50/p95/max: '+ '/'.join(f'{v:.1f}' for v in np.percentile(times,[50,95,100]))+' ms')
        if row['method']=='exact-output':
            count=sum(case.get('cancelled_facets',0) for case in row['results'])
            error=max(case.get('max_coordinate_rounding_m',0.) for case in row['results'])
            lines.append(f'  Entfernte entgegengesetzte Nullflächen: {count}; maximale Koordinatenrundung: {error:.3g} m')
        lines.append('')
    admitted=any(row['method']=='exact-output' and row['passed']==183 for row in rows)
    lines.extend(['Prismatischer Supportvergleich bestanden.' if admitted else 'Alle allgemeinen Verfahren bleiben abgelehnt.',
                  'Allgemeine Shape-/Normal-/Ressourcenverträge bleiben offen.',
                  'Keine Shape-/LOD-/Native-Freigabe, keine vollständigen Architekturrezepte.'])
    table.text(0.,.94,'\n'.join(lines),va='top',fontsize=17,linespacing=1.5)
    fig.suptitle('Boolesche Supportregionen · exakte Konstruktionen und ihre Ausgabegrenzen',fontsize=27)
    fig.tight_layout(rect=(0.,.12,1.,.95))
    report=save_plot(fig,output,'current-exact-semantic-comparison','CSG · rationale Schnittpunkte, Float64-Grenzen',
        'Geprüfter 2-mm-Schnitt links, vollständige Gegenfälle rechts. Keine allgemeine Shape-Freigabe; '
        'identische Ausgabe-Kanten werden nur im exact-rounded-Versuch kontrahiert, keine Epsilon-Verschmelzung.',
        provenance,start,[3200,1800],label='semantic-overlay')
    path=output/report;record=json.loads(path.read_text())
    record.update(full_suite_admitted=admitted,comparison_reports=[str(path) for path in reports],
        admitted_scope='closed prism support comparison only' if admitted else None)
    path.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/exact-overlay-release'))
    parser.add_argument('reports',type=Path,nargs='+');args=parser.parse_args()
    print(render(args.output,args.reports))
