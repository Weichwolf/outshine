"""Check insertion tolerances against the versioned ELF bytes selected through Ghidra."""

import argparse
import hashlib
import json
from pathlib import Path
import struct

from esri_area_reference import mapped_offset
from esri_ghidra import CORE,read_analysis


VALUES=(('scope-zero-m',0x905430,1e-4),('ratio-denominator-m',0x905258,1e-8),
        ('geometry-zero-m',0x905270,1e-25),('ratio-fallback',0x905c90,1.),
        ('ratio-average',0x905cf0,.5),('matrix-identity',0x905c88,1.),
        ('negative-minimum-size-m',0x905320,-1e-5),('positive-minimum-size-m',0x905328,1e-5))


def inspect(core,reference):
    analysis=read_analysis(reference,'insertion constants');data=core.read_bytes();digest=hashlib.sha256(data).hexdigest()
    if digest!=analysis['binary_sha256'] or analysis['sdk_version']!=[3,4,12206]:
        raise ValueError('Insertion addresses require the inspected SDK 3.4.12206 binary')
    values=[]
    for name,address,expected in VALUES:
        offset=mapped_offset(core,address-0x100000);raw=data[offset:offset+8];actual=struct.unpack('<d',raw)[0]
        if actual!=expected:raise ValueError('Insertion constant changed: '+name)
        values.append(dict(name=name,ghidra_address=hex(address),file_offset=offset,hex_bytes=raw.hex(),float64=actual))
    return dict(status='pass',binary_path=str(core),binary_sha256=digest,ghidra_commit=analysis['commit'],constants=values,
        limits='Numeric references and selected function paths only; C++ field types and isolated runtime insertion rules remain unverified')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--core',type=Path,default=CORE)
    parser.add_argument('--reference',type=Path,default=Path('build/experiments/buildings/esri-ghidra'))
    args=parser.parse_args();record=inspect(args.core,args.reference)
    (args.reference/'insert-constants.json').write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',constants=len(record['constants']))))


if __name__=='__main__':main()
