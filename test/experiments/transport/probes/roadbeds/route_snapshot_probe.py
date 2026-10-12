"""Capture a source-bound motor route and separate traffic/building reservations."""

import argparse
import json
from pathlib import Path
import subprocess
import sys

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import transport_coupled_roadbed_experiment as coupled
from transport_cut_setbacks import restore_cut_plan
from transport_directed_routes import directed_module_routes
from transport_junction_surfaces import motor_surface,public_pedestrian_surface
from transport_joined_cut_setbacks import setback_joined_network


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--place',required=True)
    parser.add_argument('--module',type=int,required=True)
    parser.add_argument('--pair',type=int,nargs=2,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--context-reach',type=float,default=0.)
    parser.add_argument('--joined-setback',type=float)
    parser.add_argument('--sections',type=int,nargs='+')
    args=parser.parse_args();root=Path('build/experiments/transport');name=args.place
    if not np.isfinite(args.context_reach) or args.context_reach<0:parser.error('context reach must be finite and nonnegative')
    before_path=root/'window-1000-body'/f'{name}-routes.json'
    before=json.loads(before_path.read_text())
    frame_path=root/'boundary-optimized/hermite-routes.json'
    frames=json.loads(frame_path.read_text())
    network=coupled.prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
    cut_path=root/'cut-setbacks'/f'{name}-cut-setbacks.json'
    restore_cut_plan(network,json.loads(cut_path.read_text())['plan'])
    if args.joined_setback is not None:
        if not args.sections:parser.error('joined setback needs explicit source sections')
        network,_,_=setback_joined_network(network,args.module,set(args.sections),args.joined_setback)
    poses,_=network.poses(args.module,{})
    routes,_=directed_module_routes(network.graph,poses,network.shape(args.module),network.tree)
    pair=tuple(args.pair);route=routes[pair]
    context=route.envelope.buffer(args.context_reach) if args.context_reach else route.envelope
    floor=motor_surface(context,network.graph,network.tree,network.mesh,network.vehicle,
                        network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
    region=context.buffer(coupled.roadbed_lateral_reach(network.vehicle)+network.curbs['arc_error_m'])
    receipt_path=root/'inputs'/f'{name}-input.json'
    receipt=json.loads(receipt_path.read_text())['receipt']
    buildings,_=coupled.building_reservations(receipt,region)
    other=coupled.reservations(network,region)
    pedestrians,ids=public_pedestrian_surface(region,network.graph,network.tree)
    data=dict(entry={k:poses[pair[0]][k].tolist() for k in ('position','heading')},
        exit={k:poses[pair[1]][k].tolist() for k in ('position','heading')},
        surface=floor.wkb.hex(),reservations=buildings.union(other).difference(floor).wkb.hex(),
        buildings=buildings.wkb.hex(),non_motor_reservations=other.wkb.hex(),source_route=route.wkb.hex(),
        public_pedestrian_surface=pedestrians.wkb.hex(),public_pedestrian_edges=ids.tolist(),
        vehicle=network.vehicle,error_m=network.curbs['arc_error_m'],place=name,module=args.module,pair=pair,
        input_sha256=before['input_sha256']|{str(p.relative_to(root)):coupled.digest(p)
            for p in (before_path,frame_path,cut_path,receipt_path)},
        context_reach_m=args.context_reach,joined_setback_m=args.joined_setback,joined_sections=args.sections,
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip())
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(data,indent=2)+'\n')
    print(json.dumps(dict(place=name,module=args.module,pair=pair,source_length_m=route.length,
                         public_area_m2=floor.area,buildings_area_m2=buildings.area)))


if __name__=='__main__':main()
