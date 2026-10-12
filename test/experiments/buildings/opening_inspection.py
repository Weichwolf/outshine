"""Inspect cached room openings with a correct perspective camera and inexpensive studio shading."""

import argparse
import hashlib
import json
from pathlib import Path

from evidence import revision
from opening_probe import prepare_case,scene
from preview import run_render


def probe(label,source,output,provenance):
    openings,mesh,base=prepare_case(label,source,output,provenance)
    reference=source/f'roof-{label}-flat-near-scene.json'
    rows=[]
    for view in ('window','entrance'):
        stem=f'inspection-{label}-{view}'
        image=output/(stem+'.png')
        configuration=scene(reference,image,mesh,view,openings)
        configuration['inspection_workbench']=True
        configuration['constructive']['glass_part_ids']=[]
        result=run_render(configuration,output/(stem+'-scene.json'))
        report=base|result|dict(representation='opening-inspection',view=view,profile=configuration['profile'],
                              image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
                              inspection_glass_hidden=True,lighting_scope='Workbench studio, not the Esri daylight reference',
                              limitations=['glass hidden to inspect actual room connections; cached mesh unchanged',
                                           'generic empty rooms; no native LOD, night or lighting admission'])
        (output/(stem+'-report.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
        rows.append(report)
        print(json.dumps(dict(label=label,view=view,seconds=result['render_seconds'])),flush=True)
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=Path('build/experiments/buildings/roof-plans'))
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/opening-inspections'))
    parser.add_argument('--cases',nargs='+',default=['rectangle','vienna-compact','vienna-courtyard','tokyo-compact'])
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    provenance=revision()
    rows=[row for label in args.cases for row in probe(label,args.source,args.output,provenance)]
    (args.output/'inspection-summary.json').write_text(json.dumps(dict(**provenance,views=rows),ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':
    main()
