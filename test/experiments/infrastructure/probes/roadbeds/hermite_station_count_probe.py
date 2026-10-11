"""Compare collision sampling density with unchanged curve and evaluation budgets."""

import argparse
import inspect
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import infrastructure_hermite_route_fitting as fitting
from infrastructure_cut_setback_experiment import digest,implementation_files


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--stations',type=int,nargs='+',default=[17,33,65])
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    data=json.loads(args.input.read_text())
    poses=[{k:np.array(v) for k,v in data[p].items()} for p in ('entry','exit')]
    surface,source=(shapely.from_wkb(data[k]) for k in ('surface','source_route'))
    function=inspect.getsource(fitting.fit_hermite_route)
    pattern=inspect.getsource(fitting.residual_dependencies)
    assert function.count('np.repeat(np.arange(count-1),17)')==1
    rows=[]
    for stations in args.stations:
        if stations<3:parser.error('at least three collision stations required')
        altered=function.replace('np.repeat(np.arange(count-1),17)',f'np.repeat(np.arange(count-1),{stations})')
        altered=altered.replace('np.linspace(0,1,17)',f'np.linspace(0,1,{stations})')
        altered=altered.replace('count-1,68',f'count-1,{4*stations}').replace('count-1,17',f'count-1,{stations}')
        namespace=dict(fitting.__dict__)
        exec(compile(pattern.replace('93',str(5*stations+8))+'\n'+altered,str(Path(__file__)), 'exec'),namespace)
        for spacing in (8.,4.):
            begin=time.perf_counter()
            curve,note=namespace['fit_hermite_route'](*poses,source,surface,data['vehicle'],data['error_m'],
                spacing=spacing,maximum_calls=1024,sample_body_edges=True,include_controls=True)
            row=dict(stations=stations,spacing_m=spacing,accepted=curve is not None,
                     elapsed_ms=(time.perf_counter()-begin)*1000,residual_calls=note['residual_calls'],reason=note.get('reason'))
            rows.append(row);print(json.dumps(row),flush=True)
            if curve is not None:
                (args.output/f'{stations}-{spacing:g}-candidate.json').write_text(json.dumps(note,indent=2)+'\n')
    sources=implementation_files(Path(__file__))
    result=dict(cases=rows,input=str(args.input),input_sha256=digest(args.input),
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources},
        scope='Collision residual samples varied; same 1024 evaluations and unchanged conservative curve/body acceptance.')
    (args.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')


if __name__=='__main__':main()
