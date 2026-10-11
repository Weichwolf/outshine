"""Test functional road space with explicit bounded cuts of building reservations."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from infrastructure_building_clearance import indexed_cut
from infrastructure_cut_setback_experiment import digest,implementation_files
from infrastructure_driving_network import DrivingNetwork
from infrastructure_source_roadbeds import construct_route_roadbed
from infrastructure_turn_roadbed_experiment import building_reservations,reservations


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--place',required=True)
    parser.add_argument('--input',type=Path,nargs='+',required=True)
    parser.add_argument('--maximum-cut-fraction',type=float,default=.05)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if not 0<args.maximum_cut_fraction<1:parser.error('building cut fraction must be between zero and one')
    root=Path('build/experiments/infrastructure');args.output.mkdir(parents=True,exist_ok=True)
    receipt_path=root/'inputs'/f'{args.place}-input.json';receipt=json.loads(receipt_path.read_text())['receipt']
    rows=[];network=None
    for path in args.input:
        data=json.loads(path.read_text());vehicle,error=data['vehicle'],data['error_m']
        if network is None:network=DrivingNetwork(root,args.place,vehicle)
        assert network.vehicle==vehicle
        poses=[{k:np.array(v) for k,v in data[p].items()} for p in ('entry','exit')]
        floor,source=(shapely.from_wkb(data[k]) for k in ('surface','source_route'))
        region=source.envelope.buffer(12)
        buildings,_=building_reservations(receipt,region)
        components=shapely.get_parts(buildings)
        protected=reservations(network,region)
        begin=time.perf_counter()
        candidate,trials=construct_route_roadbed(*poses,floor,protected,source,vehicle,error)
        row=dict(input=str(path),input_sha256=digest(path),curve_found=candidate is not None,
                 elapsed_ms=(time.perf_counter()-begin)*1000,
                 residual_calls=sum(t.get('residual_calls',0) for t in trials),accepted=False,
                 reasons=[t.get('reason') for t in trials])
        if candidate is not None:
            cut,audit=indexed_cut(components,np.array([candidate['surface']],dtype=object))
            area=shapely.area(components);remaining=shapely.area(cut)
            fraction=np.maximum(0,area-remaining)/area
            row.update(building_components=len(components),maximum_cut_fraction=float(fraction.max(initial=0)),
                       total_cut_area_m2=float(np.maximum(0,area-remaining).sum()),cut_search=audit)
            valid=shapely.is_valid(cut)&~shapely.is_empty(cut)&(fraction<=args.maximum_cut_fraction)
            assert candidate['surface'].intersection(protected.difference(floor)).area<=1e-8
            if np.all(valid):
                row['accepted']=True;output=args.output/path.stem;output.mkdir(exist_ok=True)
                product=dict(candidate=candidate,building_components=[dict(original=p.wkb.hex(),adjusted=q.wkb.hex(),
                              removed_fraction=float(f)) for p,q,f in zip(components,cut,fraction)])
                def encode(value):
                    if isinstance(value,shapely.Geometry):return value.wkb.hex()
                    if isinstance(value,np.ndarray):return value.tolist()
                    if isinstance(value,np.generic):return value.item()
                    raise TypeError(type(value).__name__)
                (output/'candidate.json').write_text(json.dumps(product,indent=2,default=encode)+'\n')
                row['candidate']=str(output/'candidate.json')
        rows.append(row);print(json.dumps(row),flush=True)
    sources=implementation_files(Path(__file__))
    result=dict(cases=rows,maximum_cut_fraction=args.maximum_cut_fraction,receipt_sha256=digest(receipt_path),
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources},
        scope='Native road curves and clipped connected building reserves; per-source building identity and whole coupled publication remain required.')
    (args.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')


if __name__=='__main__':main()
