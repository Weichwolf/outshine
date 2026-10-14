"""Rejected native hole export versus complete graph facets and actual library roofs."""

import argparse
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from building_plan_views import patch
from evidence import revision
from outshine_ref.buildings.modelling import Shape,prepare_hip
from outshine_ref.buildings.modelling.skeleton_native import partition
from showcase_groups import save_plot
from showcase_roof_shed import spatial
from verify_modelling_roof_shed import source


def render(output):
    output.mkdir(parents=True,exist_ok=True);provenance=revision();started=time.perf_counter()
    fixture=json.loads((Path(__file__).parent/'reference/hip-plans.json').read_text())
    rows=[next(r for r in fixture['records'] if r['id']==identifier) for identifier in ['Vienna:112890:2','Vienna:1137690:3']]
    fig,axes=plt.subplots(2,2,figsize=(38.4,21.6));fig.suptitle('Native Hofdächer · vollständige Quellkanten aus dem Skelettgraphen',fontsize=32)
    measurements=[]
    for column,row in enumerate(rows):
        polygon=shapely.Polygon(row['outer'],row['holes']);plan=prepare_hip(polygon)
        rings=[np.asarray(r.coords)[:-1] for r in (plan.footprint.exterior,*plan.footprint.interiors)]
        nodes,faces=partition(plan.footprint,rings,1024)
        old=[shapely.Polygon(nodes[list(f)]) for f in faces]
        missing=plan.footprint.difference(shapely.union_all(old))
        for level,(title,polygons) in enumerate([('Abgewiesener SFCGAL-Facettenexport',old),('Neuer nativer Graphexport',[f.polygon for f in plan.facets])]):
            axis=axes[level,column]
            for number,p in enumerate(polygons):patch(axis,p,facecolor=plt.cm.tab20(number%20),edgecolor='#243848',linewidth=.7,alpha=.65)
            if level==0:
                for part in shapely.get_parts(missing):patch(axis,part,facecolor='#c6333f',edgecolor='#86141d',linewidth=1.)
            for ring in (plan.footprint.exterior,*plan.footprint.interiors):
                xy=np.asarray(ring.coords);axis.plot(*xy.T,color='#182b3e',linewidth=1.8)
            axis.set(aspect='equal',xlabel='Ost relativ zum Plan [m]',ylabel='Nord relativ zum Plan [m]')
            axis.set_title(f"{row['id']} · {title}\n{len(polygons)} Facetten / {len(plan.facets)} Eaves"+
                (f' · rot: {missing.area:.3f} m² fehlend' if level==0 else ' · vollständiger geprüfter Flächenbesitz'),fontsize=20)
            axis.grid(alpha=.1);axis.tick_params(labelsize=15)
        measurements.append(dict(id=row['id'],raw_facets=len(old),complete_facets=len(plan.facets),raw_missing_area_m2=missing.area))
    fig.text(.035,.025,'Linux SFCGAL 2.3.0 / CGAL 6.2. Die fehlenden Rohflächen wurden bislang durch den Python-Fallback ersetzt. '
             'Neue native Graphpartition erhält Originaleaves und bleibt gegen dieselbe Fixture geprüft.',fontsize=22)
    fig.tight_layout(rect=(0.,.065,1.,.94))
    reports=[save_plot(fig,output,'current-native-hip-graph-plan','Native Hofdächer · fehlende Facetten geschlossen',
        'Rot markiert tatsächliche Lücken des abgewiesenen nativen Facettenexports. Der neue Graphpfad rekonstruiert die vollständige '
        'Partition aus gemessenen Bisektoren und unveränderten Quellringen; Höfe bleiben offen. Keine nachträglich frei gefüllten Löcher.',
        provenance,started,[3840,2160],label='native-hip-graph')]
    for row in rows:
        polygon=shapely.Polygon(row['outer'],row['holes']);roof=source(polygon).roof_hip(3.,'byHeight')
        shapes=[(Shape.from_polygon(polygon).extrude(5.),0),(roof.translate('rel','world',0.,5.,0.),3)]
        name='current-native-hip-graph-'+row['id'].split(':')[1]
        reports.append(spatial(output,provenance,name,row['id']+' · vollständiges natives Hofdach',shapes,
            dict(source_id=row['id'],source_sha256=row['source_sha256'],plan_method=roof.roof_info.plans[0].method,
                 source_eaves=len(roof.roof_info.plans[0].facets),inspection_body_height_m=5.,inspection_roof_rise_m=3.),
            'Tatsächliche Ausgabe von Shape.roof_hip mit vollständigem nativem Quellkantenbesitz. Gold: geschlossene Dachhülle; '
            'Blau: einfacher Prüfkörper mit unveränderten Höfen. 5 m Körper / 3 m Dach sind Prüfparameter, kein gelieferter Bauplan '
            'oder fertiges Architektur-Rezept.',label='native-hip-graph',direction=(1.,-1.2,1.4)))
    (output/'native-hip-graph-missing-area.json').write_text(json.dumps(measurements,indent=2)+'\n')
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/native-hip-graph-release'))
    print(render(parser.parse_args().output))
