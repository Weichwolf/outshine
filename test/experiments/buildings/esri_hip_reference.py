"""Pinned hip/even/overhang dispatch and bounded helper exports; no full CGA oracle."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from evidence import revision
import esri_ghidra as ghidra


ADDRESSES={'006620e0','00664a20'}


def helpers(base):
    analysis=ghidra.read_analysis(base,'hip reference')
    roof=next(f for f in analysis['selected_functions'] if f['address']=='006697b0')
    selected={call['symbol']:call['signature'] for call in roof['direct_calls'] if call['address'] in ADDRESSES}
    if len(selected)!=2:raise ValueError('Expected the inspected extend/self-intersection helpers')
    return analysis,selected


def inspect(reference,base,analyze=False):
    analysis,targets=helpers(base);binary=ghidra.CORE.read_bytes();digest=hashlib.sha256(binary).hexdigest()
    if digest!=analysis['binary_sha256']:raise ValueError('Hip SDK binary differs from the pinned baseline')
    original=ghidra.TARGETS
    try:
        ghidra.TARGETS=targets
        exported=ghidra.inspect(ghidra.CORE,reference) if analyze else ghidra.read_analysis(reference,'hip helper reference')
    finally:ghidra.TARGETS=original
    if exported['binary_sha256']!=digest:raise ValueError('Hip helper export has a different binary')
    for row in exported['selected_functions']:
        if hashlib.sha256((reference/row['external_reference_file']).read_bytes()).hexdigest()!=row['external_reference_sha256']:
            raise ValueError('Hip helper export changed')
    dispatch=subprocess.check_output(['objdump','-d','--disassemble=_ZN9Processor7roofHipEN2GC13RoofValueType4EnumEddb',str(ghidra.CORE)],text=True)
    if not all(token in dispatch for token in ('%dl,%esi','%al','%xmm2,%xmm2','roofSkeleton')):
        raise ValueError('Pinned hip dispatch no longer forwards even as roof type and clears the extra extension')
    sine=subprocess.check_output(['objdump','-d','--start-address=0x7acf0','--stop-address=0x7ad00',str(ghidra.CORE)],text=True)
    if 'sin@plt' not in sine:raise ValueError('Hip overhang sine import changed')
    return revision()|dict(status='pass',binary_sha256=digest,helper_analysis_commit=exported['commit'],
                          functions=[dict(address=f['address'],instructions=f['instructions'],reference_sha256=f['external_reference_sha256'])
                                     for f in exported['selected_functions']],
                          dispatch_sha256=hashlib.sha256(dispatch.encode()).hexdigest(),
                          interpretation=dict(even='RoofType 0/1; node-fan groups average in byAngle and use group maximum in byHeight; executable geometry kernel corrects earlier static-only interpretation',
                                              overhang='Nominal slope sine used for negative extension along boundary-to-skeleton rays; separate self-intersection resolution and extra extension helpers',
                                              boundary='SDK roof.bottom denotes the original face; an extended roof is not automatically a closed extruded solid'),
                          limits='Bounded static exports, dispatcher disassembly and SDK documentation. Exact even grouping, full executable CGA output, UV extrapolation, trim and negative overhang remain unproved.')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base',type=Path,default=Path('build/experiments/buildings/esri-ghidra'))
    parser.add_argument('--reference',type=Path,default=Path('build/experiments/buildings/esri-hip-ghidra'))
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/hip-reference.json'))
    parser.add_argument('--analyze',action='store_true');args=parser.parse_args()
    record=inspect(args.reference,args.base,args.analyze);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status=record['status'],functions=record['functions'],limits=record['limits'])))
