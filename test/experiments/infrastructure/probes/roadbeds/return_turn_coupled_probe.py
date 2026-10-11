"""Prove a return-turn recipe on the shared mesh before choosing runtime search order."""

import json
from pathlib import Path
import resource
import subprocess
import sys
import time

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import infrastructure_coupled_roadbed_experiment as coupled
from infrastructure_return_roadbeds import construct_return_roadbed

original=coupled.construct_route_roadbed


def candidate(*args,**kwargs):
    result,trials=original(*args,**kwargs)
    if result is None:
        result,note=construct_return_roadbed(*args,**kwargs)
        trials.append(note)
    return result,trials


coupled.construct_route_roadbed=candidate
root=Path('build/experiments/infrastructure')
output=root/'roadbeds/return-turn-coupled-1678'
began=time.perf_counter()
report=coupled.real_case(root,'ZuerichHauptbahnhof',1678,[4,6],4.,output)
sources=coupled.implementation_files(Path(__file__))
sources += [Path('test/experiments/infrastructure/geos_triangulation.py').resolve(),
            Path('test/experiments/infrastructure/infrastructure_network_recipes.json').resolve()]
report.update(elapsed_ms=(time.perf_counter()-began)*1000,
    measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
    implementation_sha256={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in sources},
    experimental_recipe='Existing route recipes followed by bounded three-arc return-turn fitting; search order not integrated.')
(output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda obj:obj.tolist())+'\n')
print(json.dumps({k:report[k] for k in ('accepted','connections','affected','added_area_m2','elapsed_ms')}))
