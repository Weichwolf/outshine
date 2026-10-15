"""Independent compact facade coverage at several explicit filter footprints."""

import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

from evidence import revision
from outshine_ref.buildings.facades import FacadeRepeat


def render(output):
    output.mkdir(parents=True,exist_ok=True);repeat=FacadeRepeat(20.,15.,6,5,.48,.53)
    x,y=np.meshgrid(np.linspace(.01,19.99,800),np.linspace(.01,14.99,600));points=np.stack((x,y),axis=-1)
    fig,axes=plt.subplots(2,2,figsize=(24,13.5));records=[]
    for axis,foot in zip(axes.flat,((.01,.01),(.6,.6),(2.,2.),(4.,4.)),strict=True):
        coverage=repeat.coverage(points,foot)
        axis.imshow(coverage,origin='lower',extent=(0.,20.,0.,15.),cmap='Blues',vmin=0.,vmax=1.)
        axis.set_title(f'Filter {foot[0]:.2f}×{foot[1]:.2f} m · Fensteranteil aus demselben Achsenplan')
        axis.set(xlabel='Fassadenachse [m]',ylabel='Höhe ab Gebäudesockel [m]')
        records.append(dict(footprint_m=foot,minimum=float(coverage.min()),maximum=float(coverage.max()),mean=float(coverage.mean())))
    fig.suptitle('Kompakte Geschoss-/Fensterachsen · explizit gefilterte unbeleuchtete Flächenanteile',fontsize=23)
    fig.text(.04,.04,'Blau: geschätzter Fensterflächenanteil, Weiß: Wand. Fenstergeometrie wird hierfür nicht aufgebaut.\n'
        'Filterboxen sind eine deklarierte Näherung der Projektion; echte Nahöffnungen/Räume und native Shader-/Frame-Abnahme bleiben eigene Anforderungen.',fontsize=15)
    fig.tight_layout(rect=(0.,.13,1.,.93));path=output/'facade-filter-plan.png';fig.savefig(path,dpi=120);plt.close(fig)
    (output/'facade-filter-plan.json').write_text(json.dumps(revision()|dict(profile=[2880,1620],cases=records),indent=2)+'\n');print(path)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/facade-controls'))
    render(parser.parse_args().output)
