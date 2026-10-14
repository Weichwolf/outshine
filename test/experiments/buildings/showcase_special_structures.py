"""Metric source-footprint inspections and decision limits for special structures."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from evidence import revision
from showcase_groups import save_plot


COLOURS={'ordinary':'#a0a8ad','chimney':'#d36137','mast':'#dca02b','tower':'#287d91',
         'tower_section':'#529cb2','cooling_tower':'#637d44','industrial_round_body':'#8267aa',
         'storage_tank':'#8267aa','silo':'#b08863'}
LABELS={'ordinary':'Gebäudekörper','chimney':'Schornstein','mast':'Mast / Antenne','tower':'Turm',
        'tower_section':'Turmabschnitt','cooling_tower':'Kühlturm','industrial_round_body':'Industrie-Rundkörper',
        'storage_tank':'Tankkörper','silo':'Silo'}
FOCAL=(('Spittelau','building:11451449662'),('Donauturm','building:2116140532'),
       ('BerlinFernsehturm','building:3978916422'),('Brocken','building:2017362542'),
       ('Drax','building:968472140'),('Grohnde','building:312561210'),
       ('TokyoSkytree','building:2882691470'),('Grohnde','building:1061412372'))


def footprint_plot(features,output,proof,provenance):
    started=time.perf_counter();fig,axes=plt.subplots(2,4,figsize=(24,14),layout='constrained')
    for axis,(site,source) in zip(axes.flat,FOCAL,strict=True):
        candidates=[f for f in features if f['site']==site and f['source_id']==source and not f['clipped']]
        selected=max(candidates,key=lambda f:f['decision']['metrics']['area_m2'])
        polygon=shapely.from_wkb(selected['polygon_wkb']);anchor=polygon.centroid
        for feature in features:
            if feature['site']!=site or feature['decision']['kind']=='omit':continue
            p=shapely.from_wkb(feature['polygon_wkb'])
            if p.distance(anchor)>max(35.,polygon.bounds[2]-polygon.bounds[0]):continue
            xy=np.asarray(p.exterior.coords)-[anchor.x,anchor.y]
            kind=feature['decision']['kind'];axis.fill(*xy.T,color=COLOURS.get(kind,'#a0a8ad'),alpha=.5,edgecolor='white',linewidth=.6)
        xy=np.asarray(polygon.exterior.coords)-[anchor.x,anchor.y];axis.plot(*xy.T,color='#172a35',linewidth=1.8)
        d=selected['decision'];m=d['metrics'];extent=max(12.,m['length_m']*.75)
        axis.set(xlim=(-extent,extent),ylim=(-extent,extent),aspect='equal',xlabel='Ost [m]',ylabel='Nord [m]')
        axis.grid(alpha=.15);axis.set_title(site+' · '+LABELS[d['kind']]+'\n'+f"{m['width_m']:.1f} × {m['length_m']:.1f} m · {m['bottom_m']:g}–{m['top_m']:g} m",fontsize=13)
    fig.suptitle('Sonderformen aus echten Provider-Grundrissen und Höhen\nFarben sind plausible Interpretationen; keine gelieferte Bauwerksart',fontsize=19)
    report=save_plot(fig,output,'current-special-footprints','Sonderbauten · echte Grundrisse',
        'OpenFreeMap/OpenMapTiles, derselbe festgehaltene Anbieterstand wie Outshine. Spittelau, Donauturm, Berlin, '
        'Brocken, Drax, Grohnde und Tokyo: Umrisse und Höhen stammen vom Provider; Farben zeigen die Heuristik. '
        'Grohndes großer Rundkörper bleibt ausdrücklich mehrdeutig. Keine ortsspezifischen Regeln.',provenance,started,[2400,1400],label='special-structures')
    path=output/report;r=json.loads(path.read_text());r.update(proof=proof);path.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n')
    return report


def diagnostic_plot(features,output,proof,provenance):
    started=time.perf_counter();fig,axes=plt.subplots(1,2,figsize=(24,14),layout='constrained')
    for kind,colour in COLOURS.items():
        values=[f['decision']['metrics'] for f in features if f['decision']['kind']==kind and not f['clipped']]
        if not values:continue
        axes[0].scatter([m['length_m'] for m in values],[m['height_m'] for m in values],s=18 if kind=='ordinary' else 55,
            color=colour,alpha=.25 if kind=='ordinary' else .8,label=LABELS[kind])
    axes[0].set(xscale='log',yscale='log',xlabel='Längste Grundrissachse [m]',ylabel='Gelieferte Körperhöhe [m]',
        title='Quellformen über elf Prüfräume');axes[0].grid(alpha=.15);axes[0].legend(loc='upper left',fontsize=12)
    axis=axes[1];axis.axis('off')
    lines=['Erkennung und Grenzen','',
        f"{proof['passed']} / {proof['cases']} bekannte Struktur-Prüffälle passen",'',
        'Hohe kompakte Schächte → Mast / Turm',
        'Schlanker Schaft + Industriegebiet → Schornstein',
        'Großer hoher Rundkörper + Industrie → Kühlturm',
        'Gelieferte Unter-/Oberkante trennt Turmabschnitte','',
        'Lange schmale Häuser bleiben Gebäudekörper.',
        'Kachelfragmente belegen keine Sonderform.',
        'Namen und Ortskoordinaten steuern keine Klasse.','',
        'Nicht eindeutig aus diesen Angaben:',
        'Wasserturm vs. Aussicht-/Funk-/Wohnturm',
        'Antennenfunktion, Reaktorgebäude, genaue Silhouette','',
        'Im Prüfsatz keine passenden POI-Typen für:',
        'Antennen, Schornsteine, Strommasten, Schüsseln',
        'Effelsbergs Schüssel fehlt als nutzbarer Quellkörper.','',
        'Regression nach Ortsprüfung; keine globale Trefferquote.']
    axis.text(.02,.98,'\n'.join(lines),va='top',fontsize=17,linespacing=1.5,color='#253542')
    name=save_plot(fig,output,'current-special-diagnostics','Sonderbauten · Entscheidungen und Datenlücken',
        'Alle ausgewerteten Quellkörper mit ihrer metrischen Höhe. Bekannte Orte prüfen strukturelle Familien; '
        'sie beweisen keine exakten OSM-Typen. Ungelabelte Treffer bleiben prozedurale Interpretationen mit Alternativen. '
        'Die Datenlücken verhindern derzeit eine genaue Klassifikation jeder Sonderbauwerksart.',provenance,started,[2400,1400],label='special-structures')
    path=output/name;r=json.loads(path.read_text());r.update(proof=proof);path.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n')
    return name


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,default=Path('build/experiments/buildings/special-structures/classification.json'))
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/special-structures'))
    args=parser.parse_args();report=json.loads(args.input.read_text());provenance=revision()
    if report['status']!='pass' or report['commit']!=provenance['commit'] or report['implementation_sha256']!=provenance['implementation_sha256']:
        raise ValueError('Plots require a current passing source survey')
    proof={k:report[k] for k in ('cases','passed','status','seconds','process_peak_rss_mib','scope')}
    proof['survey_sha256']=hashlib.sha256(args.input.read_bytes()).hexdigest()
    args.output.mkdir(parents=True,exist_ok=True)
    names=[footprint_plot(report['features'],args.output,proof,provenance),diagnostic_plot(report['features'],args.output,proof,provenance)]
    print(json.dumps(dict(reports=names)))
