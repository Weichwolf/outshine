"""Check a forward curve after explicit construction of unusable generated islands."""

import argparse
import json
from pathlib import Path
import subprocess
import sys

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from infrastructure_cut_setback_experiment import digest,implementation_files
from infrastructure_fitted_roadbeds import spline_roadbed
from infrastructure_roadbed_void_recipe import close_unusable_voids,verify
from steering_roadbed_candidate_probe import candidate


def render(result,data,output):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from infrastructure_building_clearance import plot_geometry
    fig,axes=plt.subplots(1,2,figsize=(13,6))
    path=shapely.LineString(result['path']);source=shapely.from_wkb(data['source_route'])
    x0,y0,x1,y1=source.bounds
    for ax,key,title in zip(axes,('original_surface_wkb','planned_surface_wkb'),('Previous roadbed','Local constructed roadbed')):
        ax.set_facecolor('#f3efe3')
        plot_geometry(ax,[shapely.from_wkb(result[key])],'#9bacaa')
        if key=='planned_surface_wkb':
            plot_geometry(ax,[shapely.from_wkb(w) for w in result['recipe']['filled_voids_wkb']],'#d4984f')
            ax.plot(*path.xy,color='#176994',linewidth=2,label='Checked vehicle path')
        else:ax.plot(*source.xy,'--',color='#9e4949',label='Source route')
        ax.set_xlim(x0-5,x1+5);ax.set_ylim(y0-5,y1+5);ax.set_aspect('equal');ax.set_title(title)
        ax.set_xlabel('east [m]');ax.set_ylabel('north [m]');ax.legend(loc='upper left')
    fig.suptitle('Local forward curve and mesh checked\n'
        f"{result['recipe']['closed_area_m2']:.2f} m² generated islands closed; complete coupled network check still pending")
    fig.tight_layout();output.parent.mkdir(parents=True,exist_ok=True);fig.savefig(output,dpi=150);plt.close(fig)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--curve',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--image',type=Path)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    data=json.loads(args.input.read_text());curve=json.loads(args.curve.read_text())
    surface,reserved=(shapely.from_wkb(data[k]) for k in ('surface','reservations'))
    support,construction=spline_roadbed(np.array(curve['curve_controls']),data['vehicle'],data['error_m'])
    assert support is not None,construction
    floor,recipe=close_unusable_voids(surface,support,reserved)
    product,_=candidate(dict(data,surface=floor.wkb.hex()),curve)
    result=dict(checks=verify(),input=str(args.input),input_sha256=digest(args.input),
        curve=str(args.curve),curve_sha256=digest(args.curve),recipe=recipe,
        original_surface_wkb=surface.wkb.hex(),normalized_surface_wkb=floor.wkb.hex(),
        planned_surface_wkb=product['surface'].wkb.hex(),path=shapely.get_coordinates(product['path']).tolist(),
        construction=product['construction'],mesh=product['mesh_audit'],
        added_roadbed_area_m2=product['addition'].area,
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in implementation_files(Path(__file__))},
        scope='Local forward curve on constructed mesh; complete coupled product and previous routes remain unverified.')
    (args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    if args.image:render(result,data,args.image)
    print(json.dumps(dict(closed_voids=recipe['closed_voids'],closed_area_m2=recipe['closed_area_m2'],
        added_roadbed_area_m2=result['added_roadbed_area_m2'],continuous_body_on_mesh=True)))


if __name__=='__main__':main()
