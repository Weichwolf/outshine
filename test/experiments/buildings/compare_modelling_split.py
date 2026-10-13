"""Export identical fixtures or compare all cut payloads with a frozen predecessor."""

import argparse
import json
from pathlib import Path

import numpy as np

from evidence import revision
from modelling_extrude_probe import fixtures
from outshine_ref.buildings.modelling.shape import Shape
from outshine_ref.buildings.modelling.split_sizes import Part,Floating


def products():
    rows=[]
    for label,polygon,elevation,height,_ in fixtures():
        parent=Shape.from_polygon(polygon,elevation,material=7,tags=(label,)).extrude(height,'world.up')
        for axis in ('x','y','z'):
            for child in parent.split(axis,*[Part(Floating(1.),str(i)) for i in range(4)]):
                rows.append(dict(label=label,axis=axis,index=child.split_info.index,source_faces=child.split_info.source_faces,
                    fields={name:value.tolist() for name,value in child.emit().items()},scope=child.scope.matrix.tolist(),
                    face_attributes=[dict(material=face.material,group=face.group,tags=sorted(face.tags),outer=face.outer,holes=face.holes,uvs=face.uvs) for face in child.geometry.polygons]))
    return rows


def compare(old,new):
    if len(old)!=len(new):raise AssertionError('Cut piece count changed')
    maximum=0.
    for before,after in zip(old,new,strict=True):
        for name in ('label','axis','index','source_faces'):
            if before[name]!=after[name]:raise AssertionError('Source identity changed: '+name)
        if before['fields'].keys()!=after['fields'].keys():raise AssertionError('Native field contract changed')
        for name,value in before['fields'].items():
            a,b=np.asarray(value),np.asarray(after['fields'][name])
            if a.shape!=b.shape:raise AssertionError('Native field shape changed: '+name)
            if a.dtype.kind in 'iu':
                if not np.array_equal(a,b):raise AssertionError('Primitive/source indices changed: '+name)
            else:
                if a.size:maximum=max(maximum,float(np.max(abs(a-b))))
                if not np.allclose(a,b,rtol=0,atol=1e-10):raise AssertionError('Native geometry/property changed: '+name)
        if not np.allclose(before['scope'],after['scope'],rtol=0,atol=1e-10):raise AssertionError('Scope matrix changed')
        if len(before['face_attributes'])!=len(after['face_attributes']):raise AssertionError('Logical face count changed')
        for a,b in zip(before['face_attributes'],after['face_attributes'],strict=True):
            for name in ('material','group','tags','outer','holes'):
                if a[name]!=b[name]:raise AssertionError('Logical face contract changed: '+name)
            for u,v in zip(a['uvs'],b['uvs'],strict=True):
                if not np.allclose(u,v,rtol=0,atol=1e-10):raise AssertionError('Logical corner UV changed')
    return dict(status='pass',pieces=len(new),max_numeric_delta=maximum,
        scope='all emitted fields, indices, materials/groups/tags, logical boundary/UV topology, source ownership and scopes')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--baseline',type=Path)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/split-baseline-comparison.json'))
    args=parser.parse_args();rows=json.loads(json.dumps(products()))
    result=revision()|compare(json.loads(args.baseline.read_text()),rows) if args.baseline else rows
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(status='pass',pieces=len(rows),mode='comparison' if args.baseline else 'export')))


if __name__=='__main__':main()
