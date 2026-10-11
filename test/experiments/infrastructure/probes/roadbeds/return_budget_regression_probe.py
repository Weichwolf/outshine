"""Compare bounded source/return selection with the previous source-only fit."""

from pathlib import Path
import json
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import infrastructure_coupled_roadbed_experiment as coupled
import infrastructure_source_roadbeds as source

original=coupled.construct_route_roadbed

def compare(*values,**options):
    result,trials=original(*values,**options)
    legacy,note=source.construct_fitted_roadbed(*values,**options)
    def brief(candidate):
        if candidate is None:return None
        return {k:v for k,v in candidate['report'].items() if k in ('reason','residual_calls','added_area_m2')}
    print(json.dumps(dict(entry=values[0]['position'].tolist(),exit=values[1]['position'].tolist(),
                         heading_dot=float(values[0]['heading']@values[1]['heading']),selected=brief(result),legacy=brief(legacy),trials=[{k:v for k,v in t.items() if k in ('reason','residual_calls')} for t in trials])),flush=True)
    return result,trials

coupled.construct_route_roadbed=compare
coupled.real_case(Path('build/experiments/infrastructure'),'ZuerichHauptbahnhof',2135,[5,3],4,
                  Path('build/experiments/infrastructure/roadbeds/return-budget-regression-2135'),None)
