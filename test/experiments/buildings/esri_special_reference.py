"""Fresh bounded primitive exports; no claim of bundled special-building recipes."""

import argparse
import json
from pathlib import Path

import esri_ghidra
from esri_offset_oracle import DIGEST


TARGETS = {
    '_ZN9Processor17primitiveCylinderEddd':'Explicit radius/height cylinder placement and insertion',
    '_ZN9Processor13primitiveConeEddd':'Explicit radius/height cone placement and insertion',
    '_ZN4util12CylinderMeshC2ERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEmbb.constprop.0':
        'Cylinder construction, subdivisions, caps and primary texture coordinates',
    '_ZN9Processor5taperEd':'CGA taper entry and geometry dispatch',
}


if __name__=='__main__':
    import hashlib
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/esri-special-ghidra'))
    args=parser.parse_args()
    if hashlib.sha256(esri_ghidra.CORE.read_bytes()).hexdigest()!=DIGEST:
        raise ValueError('Special structure reference requires pinned SDK3.4.12206')
    esri_ghidra.TARGETS=TARGETS
    result=esri_ghidra.inspect(esri_ghidra.CORE,args.output)
    result['scope']='Primitive composition reference only; no dedicated special-building recipes in installed SDK.'
    (args.output/'ghidra-core.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(selected=len(result['selected_functions']),seconds=result['elapsed_seconds'],status='pass')))
