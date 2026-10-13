"""Read pinned SDK projection-axis and numerical constants; this is not a projection oracle."""

import argparse
import hashlib
import json
from pathlib import Path
import struct

from esri_area_reference import mapped_offset
from esri_ghidra import CORE,read_analysis


def inspect(core,reference):
    analysis=read_analysis(reference,'projection constants');data=core.read_bytes()
    digest=hashlib.sha256(data).hexdigest()
    if digest!=analysis['binary_sha256'] or analysis['sdk_version']!=[3,4,12206]:
        raise ValueError('Projection addresses require the inspected SDK 3.4.12206 binary')
    tables=[]
    for address,expected in ((0x7e9fa0,(0,0,1,1,2,2)*2),(0x7e9f60,(1,2,0,2,0,1)*2)):
        offset=mapped_offset(core,address);values=struct.unpack_from('<12i',data,offset)
        if values!=expected:raise ValueError('SDK projection axis table changed')
        tables.append(dict(elf_address=hex(address),ghidra_address=hex(address+0x100000),values=list(values)))
    constants=[]
    for address,expected in ((0x905258,1e-8),(0x905538,-1e-8),(0x905230,1000.),(0x905270,1e-25),(0x9053c8,4503599627370496.)):
        offset=mapped_offset(core,address-0x100000);value=struct.unpack_from('<d',data,offset)[0]
        if value!=expected:raise ValueError('SDK projection numerical constant changed')
        constants.append(dict(ghidra_address=hex(address),value=value))
    return dict(status='pass',binary_sha256=digest,ghidra_commit=analysis['commit'],tables=tables,constants=constants,
        axes=['xy','xz','yx','yz','zx','zy']*2,
        interpretation='Tables select dimensions used for relative/floating projection sizes; six scope selectors followed by six world selectors.',
        limits='Raw table and numerical evidence only; matrix signs, offsets, inherited frames, enum registration and runtime projectUV remain unproved.')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core',type=Path,default=CORE)
    parser.add_argument('--reference',type=Path,default=Path('build/experiments/buildings/esri-ghidra'))
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/projection-constants.json'))
    args=parser.parse_args();record=inspect(args.core,args.reference)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',tables=len(record['tables']),constants=len(record['constants']))))
