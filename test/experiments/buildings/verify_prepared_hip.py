"""Existing independent roof-mesh proofs, using only plans from the owned worker."""

import argparse
import json
from pathlib import Path

from evidence import revision
from outshine_ref.buildings.modelling import HipPlanner,Shape,hip_footprints
from verify_modelling_roof_hip import controls,real_inputs


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--even',action='store_true')
    parser.add_argument('--overhang',type=float,default=0.);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    original=Shape.roof_hip
    with HipPlanner() as planner:
        def prepared(shape,*values,**options):
            plans=tuple(planner.prepare(p) for p in hip_footprints(shape))
            return original(shape,*values,**options,prepared_plans=plans)
        Shape.roof_hip=prepared
        try:control=controls(args.even,args.overhang);real=real_inputs(args.even,args.overhang)
        finally:Shape.roof_hip=original
    report=revision()|dict(status='pass',even=args.even,overhang_m=args.overhang,controls=control,real_cases=real,
        scope='All plans through desktop HipPlanner; existing independent source-frame/UV/normal/closure/volume mesh verification. Meshing remains in parent, no iOS/frame/hard mesh deadline claim.')
    args.output.write_text(json.dumps(report,indent=2)+'\n');print('pass',len(control),'controls',len(real),'real')
