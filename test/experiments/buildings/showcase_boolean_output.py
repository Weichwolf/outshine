"""Physical courtyard union using checked exact support and Float64 output repair."""

import argparse
import json
from pathlib import Path

from boolean_semantic_overlay import overlay
from boolean_semantic_attributes import attributes,cut_edges
from evidence import revision
from showcase_boolean_attributes import render_product
from verify_boolean_semantic_overlay import support_coverage,check
from verify_boolean_surfaces import controls,reference,check_uv


def render(output):
    output.mkdir(parents=True,exist_ok=True);provenance=revision();captured=[]
    _,a,b=next(row for row in controls() if row[0]=='courtyard')

    def process(native):
        result=overlay(native,method='exact-output');check_uv(result.product)
        check(result,attributes(result));support_coverage(result);edges=cut_edges(result)
        captured.append((result,edges));return result.product

    name=render_product(output,provenance,'current-output-courtyard','CSG · Hof mit darüberliegendem Verbindungsbau',
        a.shape(),b.shape(),'union',reference(a,b,'union'),transform=process)
    path=output/name;record=json.loads(path.read_text());result,edges=captured[0]
    record.update(caption='Tatsächlich erzeugte Hof-Vereinigung mit höherem Verbindungsbau. '
        'Rationale Supportregionen, Material A bei Doppelherkunft, bool.A+B und abgeleitete Schnittkanten. '
        'Geschlossene Geometrie, Volumen UND Oberfläche, Source-UV0–9 und Attribute geprüft. '
        'Metallisch bronze und keramisch grün sind Prüfmaterialien; vollständige Architekturrezepte bleiben offen.',
        label='semantic-overlay',geometry_scope='experimental closed prism semantic CSG, exact-output',
        output_repair=result.output_repair,cut_edges=len(edges))
    path.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    return name


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/output-overlay-release'))
    print(render(parser.parse_args().output))
