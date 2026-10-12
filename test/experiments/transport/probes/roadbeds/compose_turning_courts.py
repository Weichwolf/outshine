"""Reprove several local driving curves on a single turning-court mesh."""

import argparse
import json
from pathlib import Path
import subprocess
import sys

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from transport_cut_setback_experiment import digest,implementation_files
from transport_spline_routes import curve_bounds
from transport_turn_roadbed_experiment import mesh_coverage
from transport_turn_roadbeds import preserves_coverage,preserves_voids


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',type=Path,action='append',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();parts=[];pieces=[];inputs={};floor=None;vehicle=None;error=None
    for case in args.case:
        report_path=case/'results.json';report=json.loads(report_path.read_text())
        assert report['cases'][0]['accepted'],'first court is not proved'
        data_path=Path(report['input']);assert digest(data_path)==report['input_sha256']
        for p,value in report['implementation_sha256'].items():assert digest(Path(p))==value
        data=json.loads(data_path.read_text());geometry_path=case/'0-geometry.json';curve_path=case/'0-candidate.json'
        geometry=json.loads(geometry_path.read_text());note=json.loads(curve_path.read_text());current=shapely.from_wkb(geometry['original'])
        if floor is None:floor=current;vehicle=data['vehicle'];error=data['error_m']
        assert current.equals_exact(floor,0) and vehicle==data['vehicle'] and error==data['error_m']
        controls=np.array(note['curve_controls']);parts.append(shapely.from_wkb(geometry['planned']))
        pieces.append(dict(profile='G2-quintic-spline',fit=dict(controls=controls,
            maximum_curvature_per_m=np.array([curve_bounds(controls)[2].max()])),
            begin=np.array(data['entry']['position']),end=np.array(data['exit']['position'])))
        inputs.update({str(p):digest(p) for p in (report_path,data_path,geometry_path,curve_path)})
    planned=shapely.union_all(parts);assert preserves_voids(floor,planned)
    audit,actual=mesh_coverage(planned,vehicle,pieces,error);assert preserves_coverage(floor,actual)
    buildings=shapely.from_wkb(data['buildings']);args.output.mkdir(parents=True,exist_ok=True)
    sources=implementation_files(Path(__file__).resolve().parents[2]/'transport_turn_roadbed_experiment.py')+[Path(__file__).resolve()]
    result=dict(audit=audit,added_area_m2=actual.difference(floor).area,
        building_clearance_m2=actual.intersection(buildings).area,input_sha256=inputs,
        planned_surface_wkb=actual.wkb.hex(),building_resolution_pending=True,
        implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources},
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        scope='Selected local motor directions share one court mesh; shared cuts, neighboring owners and Buildings integration remain unproved.')
    (args.output/'proof.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:result[k] for k in ('audit','added_area_m2','building_clearance_m2')}))


if __name__=='__main__':main()
