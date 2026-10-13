"""Decode version-pinned area-split admission constants referenced by the local Ghidra export."""

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess

from esri_ghidra import CORE,read_analysis


DOUBLES=(('normal-cosine',0x9055f8,math.cos(math.pi/6)),
         ('radians-to-degrees',0x905260,180/math.pi),('axis-minimum-degrees',0x905560,30.),
         ('angle-half',0x905288,.5),('angle-pi',0x905460,math.pi))
MESSAGES=((0x91a1a0,'operation failed: empty geometry.'),
          (0x91a228,'operation failed: split direction too close to surface normal.'),
          (0x91a328,'operation failed: multi-face geometry is too non-planar.'))


def mapped_offset(core,address):
    for line in subprocess.check_output(['readelf','-lW',str(core)],text=True).splitlines():
        fields=line.split()
        if fields and fields[0]=='LOAD':
            offset,virtual,size=int(fields[1],16),int(fields[2],16),int(fields[4],16)
            if virtual<=address<virtual+size:return offset+address-virtual
    raise ValueError('Reference address is outside file-backed ELF segments')


def inspect(core,reference):
    analysis=read_analysis(reference,'area constants');data=core.read_bytes();digest=hashlib.sha256(data).hexdigest()
    if digest!=analysis['binary_sha256'] or analysis['sdk_version']!=[3,4,12206]:
        raise ValueError('Area constant addresses require the inspected SDK 3.4.12206 binary')
    rows=[]
    for name,address,expected in DOUBLES:
        offset=mapped_offset(core,address-0x100000);raw=data[offset:offset+8];value=struct.unpack('<d',raw)[0]
        if not math.isclose(value,expected,rel_tol=1e-14,abs_tol=1e-14):raise ValueError('Area constant changed: '+name)
        rows.append(dict(name=name,ghidra_address=hex(address),elf_address=hex(address-0x100000),
                         file_offset=offset,hex_bytes=raw.hex(),float64=value))
    messages=[]
    for address,expected in MESSAGES:
        offset=mapped_offset(core,address-0x100000);end=offset
        while end<offset+1024 and data[end:end+4]!=b'\0\0\0\0':end+=4
        value=data[offset:end].decode('utf-32-le')
        if value!=expected:raise ValueError('Area admission message changed')
        messages.append(dict(ghidra_address=hex(address),file_offset=offset,text=value))
    return dict(status='pass',binary_path=str(core),binary_sha256=digest,ghidra_commit=analysis['commit'],
        constants=rows,messages=messages,
        limits='Numeric and UTF-32 references only; decompiler types, exact plane averaging and runtime SDK area/UV equivalence remain unproved')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core',type=Path,default=CORE)
    parser.add_argument('--reference',type=Path,default=Path('build/experiments/buildings/esri-ghidra'))
    args=parser.parse_args();record=inspect(args.core,args.reference)
    (args.reference/'area-projection-constants.json').write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',constants=len(record['constants']),messages=len(record['messages']))))


if __name__=='__main__':main()
