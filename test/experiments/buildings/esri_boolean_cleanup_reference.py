"""Bounded exports for observed post-Boolean zero/coplanar/collinear cleanup calls."""

import argparse
import json
from pathlib import Path
import subprocess

import esri_ghidra
from esri_offset_oracle import DIGEST


def targets(core):
    import hashlib
    if hashlib.sha256(core.read_bytes()).hexdigest()!=DIGEST:
        raise ValueError('Cleanup reference requires pinned SDK3.4.12206')
    prefixes=('util::Mesh::removeZeroEdgesAndZeroHeights(',
              'util::Mesh::removeEdgesCoplanar(', 'util::Mesh::removeColinearPoints(')
    selected={}
    for row in subprocess.check_output(['nm','-S','--defined-only',str(core)],text=True).splitlines():
        name=row.split()[-1]
        if name.endswith('.cold') or not name.startswith('_ZN4util4Mesh'):continue
        signature=subprocess.check_output(['c++filt',name],text=True).strip()
        if signature.startswith(prefixes):selected[name]='Observed Boolean construct cleanup: '+signature
    if len(selected)!=3:raise ValueError('Expected three observed cleanup functions')
    return selected


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/esri-boolean-cleanup-ghidra'))
    args=parser.parse_args();esri_ghidra.TARGETS=targets(esri_ghidra.CORE)
    esri_ghidra.SCRIPT=esri_ghidra.SCRIPT.replace('decompileFunction(function,25,monitor)',
        'decompileFunction(function,90,monitor)')
    result=esri_ghidra.inspect(esri_ghidra.CORE,args.output)
    result['decompile_limit_seconds_per_function']=90
    (args.output/'ghidra-core.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Fresh bounded cleanup exports:',len(result['selected_functions']),flush=True)
