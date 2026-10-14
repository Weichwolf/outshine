"""Bounded Ghidra exports for the pinned external SDK's Boolean dispatch/kernel."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

import esri_ghidra
from esri_offset_oracle import DIGEST


def targets(core):
    if hashlib.sha256(core.read_bytes()).hexdigest()!=DIGEST:
        raise ValueError('Boolean study requires pinned SDK3.4.12206 core')
    selected={}
    for row in subprocess.check_output(['nm','-S','--defined-only',str(core)],text=True).splitlines():
        name=row.split()[-1]
        if name.endswith('.cold') or not (name.startswith('_ZN9Processor') or name.startswith('_ZN4util6BoolOp')):continue
        signature=subprocess.check_output(['c++filt',name],text=True).strip()
        if any(signature.startswith(prefix) for prefix in [
            'Processor::_union(', 'Processor::subtract(', 'Processor::intersect(', 'Processor::__boolOp(',
            'util::BoolOp::Msh::construct(', 'util::BoolOp::Msh::splitMesh(']):
            selected[name]='Boolean operation study: '+signature
    if len(selected)!=6:raise ValueError('Expected six unique SDK Boolean targets')
    return selected


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/esri-boolean-ghidra'))
    parser.add_argument('--split-only',action='store_true')
    parser.add_argument('--decompile-seconds',type=int,default=25)
    args=parser.parse_args();esri_ghidra.TARGETS=targets(esri_ghidra.CORE)
    if not 1<=args.decompile_seconds<=120:parser.error('Decompiler budget must be 1..120 seconds')
    if args.split_only:esri_ghidra.TARGETS={n:v for n,v in esri_ghidra.TARGETS.items() if '9splitMesh' in n}
    esri_ghidra.SCRIPT=esri_ghidra.SCRIPT.replace('decompileFunction(function,25,monitor)',
        'decompileFunction(function,'+str(args.decompile_seconds)+',monitor)')
    original=esri_ghidra.read_analysis
    def retain_failure(reference,log):
        try:return original(reference,log)
        except ValueError:
            failed=args.output/'failed-analysis'
            shutil.copytree(reference,failed,dirs_exist_ok=True)
            report=json.loads((reference/'ghidra-core.json').read_text())
            report.update(status='invalid',binary_sha256=DIGEST,log_path=str(log),decompile_limit_seconds_per_function=args.decompile_seconds)
            (failed/'failure.json').write_text(json.dumps(report,indent=2)+'\n')
            for row in report['selected_functions']:
                print(row['address'],row['name'],row.get('instructions'),row.get('decompile_completed'),
                      row.get('recovered_control_flow_bytes'),row.get('elf_symbol_bytes'),flush=True)
            raise
    esri_ghidra.read_analysis=retain_failure
    result=esri_ghidra.inspect(esri_ghidra.CORE,args.output)
    result['decompile_limit_seconds_per_function']=args.decompile_seconds
    (args.output/'ghidra-core.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Fresh bounded exports:',len(result['selected_functions']),result['binary_sha256'],flush=True)
