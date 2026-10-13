"""Retry missing routes once on jointly constructed space without cumulative widening."""

import json
from pathlib import Path
import runpy
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import transport_coupled_roadbed_experiment as coupled
from transport_shared_road_space import construct_shared_space

original=coupled.construct_module


def shared_space(network,trial,owner,sections,affected,region,buildings,section_index,required,cache=None):
    return construct_shared_space(original,network,trial,owner,sections,affected,region,buildings,section_index,required,cache)


coupled.construct_module=shared_space
state=runpy.run_path(str(Path(__file__).with_name('joint_cut_recipe_probe.py')),run_name='__main__')
report=state['report'];output=state['args'].output
sources=coupled.implementation_files(Path(__file__))
report['implementation_sha256'].update({str(p.relative_to(Path.cwd())):coupled.digest(p) for p in sources})
report['experimental_recipe']+=' Missing routes retried once on constructed public space; widening still bounded by the original floor.'
(output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda obj:obj.tolist())+'\n')
