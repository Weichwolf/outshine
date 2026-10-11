"""Prove a return-turn recipe on the shared mesh before choosing runtime search order."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import infrastructure_coupled_roadbed_experiment as coupled
import infrastructure_source_roadbeds as source
from infrastructure_return_roadbeds import construct_return_roadbed

original_fit=source.construct_fitted_roadbed


def original(*values,**options):
    result,trials=source.construct_turn_roadbed(*values,**options)
    if result is None:
        result,note=source.construct_source_roadbed(*values,**options)
        trials.append(note)
    if result is None:
        result,note=source.construct_fitted_roadbed(*values,**options)
        trials.append(note)
    return result,trials


def candidate(*values,**kwargs):
    result,trials=original(*values,**kwargs)
    if result is None and args.shared_fitting_budget is None:
        result,note=construct_return_roadbed(*values,**kwargs)
        trials.append(note)
    if args.shared_fitting_budget is not None:
        assert sum(row.get('residual_calls',0) for row in trials)<=args.shared_fitting_budget
    return result,trials


coupled.construct_route_roadbed=candidate
root=Path('build/experiments/infrastructure')
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sections',type=int,nargs='+')
parser.add_argument('--shared-fitting-budget',type=int)
parser.add_argument('--setback',type=float,default=4.)
parser.add_argument('--output',type=Path,default=root/'roadbeds/return-turn-coupled-1678')
args=parser.parse_args();output=args.output
if args.shared_fitting_budget is not None:
    if args.shared_fitting_budget<2:parser.error('shared fitting budget must be at least two')
    def bounded_fit(*values,**options):
        result,note=original_fit(*values,**options,maximum_calls=args.shared_fitting_budget//2)
        used=note['residual_calls']
        remaining=args.shared_fitting_budget-used
        if result is None and remaining>0:
            result,fallback=construct_return_roadbed(*values,**options,maximum_calls=remaining,maximum_starts=1)
            note=dict(fallback,residual_calls=used+fallback['residual_calls'],source_fit_attempt=note)
        return result,note
    source.construct_fitted_roadbed=bounded_fit
began=time.perf_counter()
report=coupled.real_case(root,'ZuerichHauptbahnhof',1678,[4,6],args.setback,output,args.sections)
sources=coupled.implementation_files(Path(__file__))
sources += [Path('test/experiments/infrastructure/geos_triangulation.py').resolve(),
            Path('test/experiments/infrastructure/infrastructure_network_recipes.json').resolve()]
report.update(elapsed_ms=(time.perf_counter()-began)*1000,
    measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
    implementation_sha256={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in sources},
    shared_fitting_budget=args.shared_fitting_budget,
    experimental_recipe=('Source fit followed by one ranked CCC/CSC seed under one residual budget' if args.shared_fitting_budget is not None
                         else 'Existing route recipes followed by separately bounded CCC/CSC fitting'))
(output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda obj:obj.tolist())+'\n')
print(json.dumps({k:report[k] for k in ('accepted','connections','affected','added_area_m2','elapsed_ms')}))
