"""Measure admitted cuts and retain a concrete rejected coplanar Manifold reference."""

import argparse
import json
import os
from pathlib import Path

import numpy as np
import shapely

from evidence import revision
from modelling_cut_manifold import encoded_source,decode_cut,volumetric_components
from modelling_extrude_probe import fixtures
from modelling_group_probe import timed
from outshine_ref.buildings.modelling.shape import Shape
from outshine_ref.buildings.modelling.split_sizes import Part,Floating
from verify_modelling_extrude import closed,volume
from verify_modelling_split import source_attributes


def rejected_reference():
    polygon=shapely.Polygon([(0,0),(12,0),(12,10),(0,10)],[[(3,2),(9,2),(9,8),(3,8)]])
    source=Shape.from_polygon(polygon,material=7).extrude(4.);reference=encoded_source(source)
    solid=reference.solid.trim_by_plane((1.,0.,0.),6.).trim_by_plane((-1.,0.,0.),-9.)
    solid,omitted=volumetric_components(reference,solid);mesh,_,_=decode_cut(reference,solid,0)
    try:closed(mesh)
    except AssertionError as error:return dict(status='rejected',reason=str(error),volume_m3=volume(mesh),
        expected_volume_m3=48.,surface_area_m2=solid.surface_area(),expected_surface_area_m2=104.,
        prior_cleanup_omissions=omitted,policy='Not exposed by Shape.split; keep as failing alternative evidence')
    raise AssertionError('Pinned coplanar reference unexpectedly became valid; reassess alternative')


def benchmark():
    rows=[];pattern=[Part(Floating(1.),str(i)) for i in range(4)]
    for label,polygon,elevation,height,source_sha in fixtures():
        source=Shape.from_polygon(polygon,elevation,material=7,tags=(label,)).extrude(height,'world.up')
        for axis in ('x','y','z'):
            call=lambda:source.split(axis,*pattern)
            products=call();meshes=[child.emit() for child in products]
            for child,mesh in zip(products,meshes,strict=True):closed(mesh);source_attributes(source,child)
            if not np.isclose(sum(volume(mesh) for mesh in meshes),polygon.area*height,rtol=1e-10,atol=1e-7):raise AssertionError('Split volume changed')
            rows.append(dict(label=label,axis=axis,method='property clipping and one-sided GEOS caps',source_sha256=source_sha,
                parts=len(products),triangles=sum(len(mesh['faces']) for mesh in meshes),
                raw_mesh_bytes=sum(array.nbytes for mesh in meshes for array in mesh.values()),**timed(call)))
    return rows


def open_benchmark():
    rows=[];pattern=[Part(Floating(1.),str(i)) for i in range(4)]
    for label,polygon,elevation,_,source_sha in fixtures():
        source=Shape.from_polygon(polygon,elevation,material=7,tags=(label,))
        for axis in ('x','z'):
            call=lambda:source.split(axis,*pattern);children=call()
            for child in children:source_attributes(source,child)
            rows.append(dict(label=label,axis=axis,method='open property clipping without caps',source_sha256=source_sha,
                parts=len(children),triangles=sum(len(child.emit()['faces']) for child in children),**timed(call)))
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/split-benchmark.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',measurements=benchmark(),open_measurements=open_benchmark(),rejected_alternative=rejected_reference(),
        warm_runs=21,openblas_threads=os.environ.get('OPENBLAS_NUM_THREADS'),
        limits='Python closed-solid generation including source validation, properties, caps, logical faces and immutable child payloads; final emitted positions excluded; no native frames/GPU or full metadata memory')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',measurements=len(record['measurements']),alternative=record['rejected_alternative']['status'])))


if __name__=='__main__':main()
