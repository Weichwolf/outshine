"""Compare identical extrusion contracts using batched and individual GEOS triangulation."""

import argparse
import json
import os
from pathlib import Path
import platform
import time
from unittest.mock import patch

import numpy as np
import shapely

from esri_oracle import CASES
from evidence import revision
import modelling_faces
from modelling_shape import Shape
from provider_contract import attributes
from samples import read_site
from verify_modelling_extrude import closed,near,volume


def individual(polygons):
    return np.asarray([shapely.constrained_delaunay_triangles(polygon) for polygon in polygons],dtype=object)


def fixtures():
    yield 'rectangle',shapely.box(0.,0.,8.,5.),0.,4.,None
    yield 'courtyard',shapely.Polygon([(0,0),(12,0),(12,10),(0,10)],[[(3,2),(9,2),(9,8),(3,8)]]),0.,4.,None
    sites={}
    for label,site,identifier in CASES:
        if site not in sites:sites[site]=read_site(site)[0]
        sample=next(row for row in sites[site] if row['id']==identifier);data=attributes(sample['properties'])
        yield label,sample['polygon'],data['minHeight'],data['bodyHeight'],sample['source_sha256']


def benchmark():
    rows=[];batch=modelling_faces.triangulations
    for label,polygon,elevation,height,source_sha in fixtures():
        began=time.perf_counter_ns();source=Shape.from_polygon(polygon,elevation,tags=(label,))
        constructor_ms=(time.perf_counter_ns()-began)/1e6;products={}
        for method,triangulate in (('batched',batch),('individual',individual)):
            with patch.object(modelling_faces,'triangulations',triangulate):
                product=source.extrude(height,'world.up');products[method]=product
                call=lambda:source.extrude(height,'world.up').emit()
                call();times=[]
                for _ in range(21):
                    began=time.perf_counter_ns();call();times.append((time.perf_counter_ns()-began)/1e6)
            mesh=product.emit();closed(mesh);near(volume(mesh),polygon.area*height,1e-5)
            rows.append(dict(label=label,method=method,p50_ms=float(np.percentile(times,50)),p95_ms=float(np.percentile(times,95)),
                             source_vertices=sum(len(ring.coords)-1 for ring in (polygon.exterior,*polygon.interiors)),
                             source_holes=len(polygon.interiors),source_sha256=source_sha,triangles=len(mesh['faces']),
                             logical_faces=len(product.geometry.polygons),raw_mesh_bytes=sum(array.nbytes for array in mesh.values()),
                             constructor_ms=constructor_ms,work='same logical caps/walls, materials, tags, corner UVs, tight scope and final mesh output; constructor excluded'))
        a,b=(products[name] for name in ('batched','individual'))
        if a.geometry.polygons!=b.geometry.polygons:raise AssertionError('Triangulation alternatives changed logical topology or inherited data')
        for field,value in a.emit().items():near(value,b.emit()[field])
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/extrusion-benchmark.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',measurements=benchmark(),warm_runs=21,
        environment=dict(machine=platform.machine(),platform=platform.platform(),numpy=np.__version__,shapely=shapely.__version__,
                         geos=shapely.geos_version_string,openblas_threads=os.environ.get('OPENBLAS_NUM_THREADS')),
        limits='Python CPU mesh generation only; no native frames, GPU time or full logical-face memory accounting')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',measurements=len(record['measurements']))))


if __name__=='__main__':main()
