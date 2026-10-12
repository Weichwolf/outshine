"""Compare face extraction with the same scopes, rings and data through a GEOS rebuild."""

import argparse
from dataclasses import replace
import json
import os
from pathlib import Path
import platform
import time
from unittest.mock import patch

import numpy as np
import shapely

from evidence import revision
from modelling_component_geometry import component_scope,subset
import modelling_components
from modelling_extrude_probe import fixtures
from modelling_faces import face_shape
from modelling_shape import Shape
from verify_modelling_components import combined,uv_signature,reconstructed_uv
from verify_modelling_extrude import near,volume


def rebuilt(surface,indices,alignment,info):
    mesh,faces=subset(surface.mesh,surface.faces,surface.triangle_rows,indices)
    scope=component_scope(mesh,faces[0],surface.normals[indices[0]],surface.source.scope,alignment)
    used=sorted({i for face in faces for ring in (face.outer,*face.holes) for i in ring});mapping={old:new for new,old in enumerate(used)}
    faces=tuple(replace(face,outer=tuple(mapping[i] for i in face.outer),holes=tuple(tuple(mapping[i] for i in ring) for ring in face.holes)) for face in faces)
    normals=mesh['normals'][used] if 'normals' in mesh else None
    return replace(face_shape(mesh['vertices'][used],faces,scope,normals,True),comp_info=info)


def benchmark():
    rows=[];reuse=modelling_components.component_shape
    for label,polygon,elevation,height,source_sha in fixtures():
        source=Shape.from_polygon(polygon,elevation,tags=(label,)).extrude(height,'world.up');products={}
        for method,constructor in (('reuse-native-triangles',reuse),('rebuild-with-geos',rebuilt)):
            with patch.object(modelling_components,'component_shape',constructor):
                call=lambda:source.comp('f')
                products[method]=call();call();times=[]
                for _ in range(21):
                    began=time.perf_counter_ns();call();times.append((time.perf_counter_ns()-began)/1e6)
            children=products[method];meshes=[child.emit() for child in children]
            rows.append(dict(label=label,method=method,source_sha256=source_sha,components=len(children),
                triangles=sum(len(mesh['faces']) for mesh in meshes),raw_mesh_bytes=sum(array.nbytes for mesh in meshes for array in mesh.values()),
                p50_ms=float(np.percentile(times,50)),p95_ms=float(np.percentile(times,95)),
                work='source surface preparation, all face selectors, adjacency, component bounds/frames, UV/tag/material payloads; emitted child positions excluded'))
        a,b=(products[name] for name in ('reuse-native-triangles','rebuild-with-geos'))
        if [child.comp_info for child in a]!=[child.comp_info for child in b]:raise AssertionError('Alternatives changed component metadata')
        for left,right in zip(a,b,strict=True):
            near(left.scope.matrix,right.scope.matrix)
            if left.geometry.polygons!=right.geometry.polygons:raise AssertionError('Alternatives changed face rings or inherited corner data')
        aa,bb=combined(a),combined(b);near(volume(aa),volume(bb),1e-5)
        if uv_signature(reconstructed_uv(a))!=uv_signature(reconstructed_uv(b)):
            raise AssertionError('Compared footprint alternatives differ in actual triangle/corner UV payloads')
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/component-benchmark.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',warm_runs=21,measurements=benchmark(),
        environment=dict(platform=platform.platform(),numpy=np.__version__,shapely=shapely.__version__,openblas_threads=os.environ.get('OPENBLAS_NUM_THREADS')),
        limits='Single positive-height prisms with planar faces and affine source UVs; no native frames, GPU time or all-CGA-input performance claim')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',measurements=len(record['measurements']))))


if __name__=='__main__':main()
