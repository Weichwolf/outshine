"""Check the pinned roof dispatcher/kernel and their ELF constants without claiming a CGA oracle."""

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct

from esri_area_reference import mapped_offset
from esri_ghidra import CORE,read_analysis


CONSTANTS=(('edge-index-rounding',0x9053c8,2.**52),('negative-angle-rotation-degrees',0x9052f8,180.),
    ('angle-limit-degrees',0x905300,90.),('minimum-angle-degrees',0x905498,1e-10),
    ('minimum-run-metres',0x905668,1.000000013351432e-10),('height-mode-reference-angle',0x905670,45.),
    ('symmetric-ridge-run-factor',0x905288,.5),
    ('degrees-to-radians',0x905490,math.pi/180.))


def inspect(core,reference):
    analysis=read_analysis(reference,'roof constants');data=core.read_bytes();digest=hashlib.sha256(data).hexdigest()
    if digest!=analysis['binary_sha256'] or analysis['sdk_version']!=[3,4,12206]:raise ValueError('Roof addresses require the inspected SDK 3.4.12206')
    records=[]
    for name,address,expected in CONSTANTS:
        position=mapped_offset(core,address-0x100000);value=struct.unpack_from('<d',data,position)[0]
        if value!=expected:raise ValueError('Roof reference constant changed: '+name)
        records.append(dict(name=name,ghidra_address=hex(address),file_offset=position,value=value))
    functions=[row for row in analysis['selected_functions'] if row['address'] in ('00526480','005225f0','005266b0')]
    dispatch={row['address'] for row in functions if any(key.startswith('005225f0 ') for key in row['direct_call_sites'])}
    if len(functions)!=3 or not {'00526480','005266b0'}<=dispatch:raise ValueError('Shed/ridge dispatcher or kernel evidence is missing')
    for row in functions:
        if hashlib.sha256((reference/row['external_reference_file']).read_bytes()).hexdigest()!=row['external_reference_sha256']:
            raise ValueError('Roof decompiler export changed')
    return dict(status='pass',binary_sha256=digest,ghidra_commit=analysis['commit'],constants=records,
        functions=[dict(address=row['address'],instructions=row['instructions'],indirect_call_sites=row['indirect_call_sites'],reference_sha256=row['external_reference_sha256']) for row in functions],
        interpretation=dict(edge_index='nearest integral float, then signed modulo exterior-ring edge count; default rounding mode assumed',
            negative_angle='absolute slope magnitude with 180 degree roof orientation rotation',
            height='positive explicit height replaces angular run scaling',scope='new-Y rotation and tight geometry bounds are called',
            ridge='same angle on both planes; height mode uses two 45 degree defaults, no selected edge and a centred ridge',
            holes='HoleSplitHelper and source tag propagation are present'),
        limits='Static bounded Ghidra exports and pinned constants; two indirect kernel calls, optimized connectivity, trim planes, exact UV/normal semantics and isolated executable CGA outputs remain unproved')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path('build/experiments/buildings/esri-ghidra'))
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/roof-reference.json'));args=parser.parse_args()
    record=inspect(CORE,args.reference);args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(dict(status='pass',functions=len(record['functions']),constants=len(record['constants']))))
