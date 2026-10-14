"""Compare eager and candidate-first repair on identical generated room inputs."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import resource
import subprocess
import time

import numpy as np

import boolean_output_repair
from evidence import revision
from verify_boolean_sequence import room


def inputs():
    records=[];original=boolean_output_repair.cancel

    def capture(roots,supports,native,nodes,**options):
        records.append((roots,supports,native,nodes))
        return original(roots,supports,native,nodes,**options)

    boolean_output_repair.cancel=capture
    try:
        room();room('subtract-sequential')
    finally:boolean_output_repair.cancel=original
    if len(records)!=4:raise ValueError('Expected four verified room/door repair inputs')
    return records


def compare(baseline_revision,output):
    source='test/experiments/buildings/boolean_output_repair.py'
    encoded=subprocess.check_output(['git','show',baseline_revision+':'+source])
    digest=hashlib.sha256(encoded).hexdigest();path=output.parent/('repair-baseline-'+digest[:12]+'.py')
    path.write_bytes(encoded);spec=importlib.util.spec_from_file_location('repair_baseline',path)
    baseline=importlib.util.module_from_spec(spec);spec.loader.exec_module(baseline)
    rows=[];kernels={'eager':baseline.cancel,'candidate-first':boolean_output_repair.cancel}
    for stage,arguments in enumerate(inputs()):
        original=arguments[0].copy();expected=baseline.cancel(*arguments)
        for repeat in range(5):
            names=('eager','candidate-first') if repeat%2==0 else ('candidate-first','eager')
            for name in names:
                start=time.perf_counter();actual=kernels[name](*arguments);elapsed=(time.perf_counter()-start)*1000
                np.testing.assert_array_equal(actual[0],expected[0]);assert actual[1]==expected[1]
                np.testing.assert_array_equal(actual[2],expected[2]);np.testing.assert_array_equal(arguments[0],original)
                rows.append(dict(stage=stage,repeat=repeat,kernel=name,repair_ms=elapsed,
                    input_triangles=len(original),output_triangles=len(actual[0])))
    return revision()|dict(status='pass',runs=rows,baseline_revision=baseline_revision,baseline_sha256=digest,
        process_peak_rss_mib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/1024,
        scope='Four identical pre-repair inputs from independently verified room/door expressions; five alternating runs per kernel/stage. Exact roots, source supports and native IDs match; inputs remain unchanged. Kernel timing excludes capture, verification and whole CSG construction. Peak RSS belongs to the combined process; no general winner or hard memory bound.')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-revision',required=True)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/rounding-repair-comparison.json'))
    args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True)
    report=compare(args.baseline_revision,args.output);args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(report['status'],len(report['runs']),'equal-output kernel measurements',flush=True)
