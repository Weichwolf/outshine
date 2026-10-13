"""Verify metric UV distances and unchanged cached building triangles and semantics."""

import argparse
import json
from pathlib import Path

import numpy as np

from evidence import revision
from facade_surface import project


def controls():
    root=Path(__file__).resolve().parents[3]/'build/experiments/buildings/opening-inspections'
    records=[]
    for label in ('rectangle','vienna-compact','vienna-courtyard','tokyo-compact'):
        with np.load(root/('opening-'+label+'-mesh.npz')) as archive:source=dict(archive)
        output,info=project(source);rows=np.flatnonzero(source['materials']==1)
        xyz=source['vertices'][source['faces'][rows]];uv=output['uv'][output['faces'][rows]]
        world=np.linalg.norm(np.roll(xyz,-1,axis=1)-xyz,axis=2)
        metric=np.linalg.norm((np.roll(uv,-1,axis=1)-uv)*[2.4,1.2],axis=2)
        np.testing.assert_allclose(metric,world,rtol=1e-9,atol=1e-9)
        np.testing.assert_array_equal(output['vertices'][output['faces']],source['vertices'][source['faces']])
        for key in ('materials','parts','levels','opening_indices'):np.testing.assert_array_equal(output[key],source[key])
        assert 'uv' not in source
        records.append(dict(label=label,max_edge_length_error_m=float(abs(metric-world).max()),**info))
    return records


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/facade-surface-verification.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',cases=controls(),
        limits='Diagnostic per-face projection; expanded corners, no persistent native UV representation, CGA equivalence or filtered LOD')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',cases=len(record['cases']))))
