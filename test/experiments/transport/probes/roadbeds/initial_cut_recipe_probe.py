"""Compare coupled cut plans with fixed initial port headings."""

import json
from pathlib import Path
import runpy
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import transport_coupled_roadbed_experiment as coupled


def initial_frames(*args,**kwargs):
    return iter(())


coupled.joint_heading_trials=initial_frames
state=runpy.run_path(str(Path(__file__).with_name('joint_cut_recipe_probe.py')),run_name='__main__')
report=state['report'];output=state['args'].output
sources=coupled.implementation_files(Path(__file__))
report['implementation_sha256'].update({str(p.relative_to(Path.cwd())):coupled.digest(p) for p in sources})
report['experimental_recipe']+=' Initial port headings kept; no shared heading search.'
(output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda obj:obj.tolist())+'\n')
