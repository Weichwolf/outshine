"""Replay a captured floor-coverage failure and reject spatially significant losses."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from infrastructure_fitted_roadbed_checks import verify_floor_coverage
from infrastructure_turn_roadbeds import coordinate_roundoff,preserves_coverage


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    snapshot=json.loads(args.input.read_text())
    before,after=(shapely.set_precision(shapely.from_wkb(snapshot[k+'_wkb']),snapshot[k+'_precision_m'],
                                      mode='pointwise') for k in ('before','after'))
    assert before.is_valid and after.is_valid
    assert preserves_coverage(before,after),'original floor lost'
    checks=verify_floor_coverage()
    sources=[Path(__file__).resolve(),*[Path(__file__).resolve().parents[2]/name for name in
        ('infrastructure_turn_roadbeds.py','infrastructure_fitted_roadbed_checks.py')]]
    digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    report=dict(checks=checks,original_floor_preserved=True,coordinate_roundoff_m=coordinate_roundoff(before),
        default_buffer_valid=bool(shapely.buffer(after,coordinate_roundoff(before),quad_segs=8).is_valid),
        input_sha256=digest(args.input),
        implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources},
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip())
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report[k] for k in ('original_floor_preserved','default_buffer_valid','coordinate_roundoff_m','checks')}))


if __name__=='__main__':
    main()
