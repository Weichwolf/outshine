"""Compare native contacts with the pinned complete Vienna mesh and 22,000 queries."""

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import sys

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT/'test/scripts/surface-contact-reference.cpp'
FIXTURE = ROOT/'test/experiments/transport/fixtures/surface-sampling-vienna-4ea502363.npz'
sys.path.insert(0,str(ROOT/'test/experiments/transport'))
from surface_contact_fixture import verify


def compile_helper(output):
    environment = os.environ.copy()
    environment['PKG_CONFIG_PATH'] = str(ROOT/'build/deps/install/lib/pkgconfig')+os.pathsep+environment.get('PKG_CONFIG_PATH','')
    libraries = shlex.split(subprocess.check_output(['pkg-config','--libs','geos'],
                                                  text=True,env=environment,timeout=30))
    binary = output/'surface-contact-reference'
    command = ['c++','-std=c++23','-O2','-ffp-contract=off','-fno-exceptions',
               '-Wall','-Wextra','-Wshadow','-Werror','-Iinclude',str(SOURCE),
               str(ROOT/'build/liboutshine.a'),*libraries,'-o',str(binary)]
    subprocess.run(command,cwd=ROOT,check=True,timeout=120)
    return binary,command


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture',type=Path,default=FIXTURE)
    parser.add_argument('--output',type=Path,default=ROOT/'build/experiments/render/native-surface-contacts')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    verified=verify(args.fixture)
    metadata=json.loads(args.fixture.with_suffix('.json').read_text())
    binary,command=compile_helper(args.output)
    with np.load(args.fixture) as data:
        vertices,faces,queries=data['vertices'],data['triangles'],data['queries_xy_m']
        packed=(struct.pack('<QQQ',len(vertices),len(faces),len(queries))+
                vertices.astype('<f8').tobytes()+faces.astype('<u4').tobytes()+queries.astype('<f8').tobytes())
        process=subprocess.run([str(binary)],input=packed,capture_output=True,check=True,timeout=30)
        (args.output/'native.tsv').write_bytes(process.stdout)
        actual=np.loadtxt(io.BytesIO(process.stdout))
        if actual.shape!=(len(queries),8):raise ValueError('native output lost queries')
        reason_map={'':0,'outside_surface':1,'ambiguous_projected_contact':2,'candidate_budget_exhausted':3}
        expected_status=np.array([reason_map[r] for r in data['reasons']])
        np.testing.assert_array_equal(actual[:,0],data['face_indices'])
        np.testing.assert_array_equal(actual[:,1],expected_status)
        np.testing.assert_allclose(actual[:,2:5],data['points_enu_m'],rtol=0,atol=metadata['native_height_tolerance_m'],equal_nan=True)
        np.testing.assert_allclose(actual[:,5:8],data['normals_enu'],rtol=0,atol=metadata['native_normal_tolerance'],equal_nan=True)
        height_error=float(np.nanmax(abs(actual[:,4]-data['points_enu_m'][:,2])))
        normal_error=float(np.nanmax(abs(actual[:,5:8]-data['normals_enu'])))
        counts=dict(vertices=len(vertices),faces=len(faces),queries=len(queries),
                    contacts=int(np.count_nonzero(expected_status==0)),outside=int(np.count_nonzero(expected_status==1)))
    report=dict(verified=True,reference=verified,counts=counts,
                maximum_height_error_m=height_error,maximum_normal_error=normal_error,
                native_timing=json.loads(process.stderr),compile_command=command,
                native_source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
                scope='Real immutable derived transport mesh; native contacts, identities and reasons. Preparation/query wall time on this host under current load; no runtime placement or GPU proof.')
    (args.output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('compile_command','reference')}))


if __name__=='__main__':
    main()
