"""Verify UV normalization's degeneracy threshold directly in the inspected SDK ELF."""

import argparse
import hashlib
import json
from pathlib import Path
import struct

from esri_area_reference import mapped_offset
from esri_ghidra import CORE,read_analysis
from modelling_uv_normalize import UV_ZERO


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,default=Path('build/experiments/buildings/esri-ghidra'));args=parser.parse_args()
    analysis=read_analysis(args.reference,'UV constants');data=CORE.read_bytes();digest=hashlib.sha256(data).hexdigest()
    if digest!=analysis['binary_sha256'] or analysis['sdk_version']!=[3,4,12206]:raise ValueError('UV addresses require the inspected SDK 3.4.12206 binary')
    values=[]
    for name,address,expected in [('uv-zero',0x905270,UV_ZERO),('uv-unit',0x905278,1.)]:
        offset=mapped_offset(CORE,address-0x100000);raw=data[offset:offset+8];actual=struct.unpack('<d',raw)[0]
        if actual!=expected:raise ValueError('UV normalization constant changed')
        values.append(dict(name=name,ghidra_address=hex(address),file_offset=offset,hex_bytes=raw.hex(),float64=actual))
    record=dict(status='pass',binary_sha256=digest,ghidra_commit=analysis['commit'],constants=values,
        limits='Numeric references and selected dispatch paths; C++ types, warning behavior and isolated runtime UV rules remain unverified')
    (args.reference/'uv-constants.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(dict(status='pass',constants=len(values))))


if __name__=='__main__':main()
