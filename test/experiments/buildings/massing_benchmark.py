"""Fresh-process exposed-band boundary versus closed-prism CSG, with identical real plans."""

import argparse
from importlib.metadata import version
import json
from pathlib import Path
import platform
import resource
import statistics
import subprocess
import sys
import time

import numpy as np
import shapely

from evidence import revision
from facade_plan import author
from massing_reference import check_measurements, union
from samples import read_site
from shell import prepare


def worker(site, method, limit):
    began = time.perf_counter()
    samples, receipt = read_site(site)
    samples = sorted(samples, key=lambda sample: sample['id'])
    if limit:
        samples = samples[:limit]
    products, rejected = [], []
    for sample in samples:
        try:
            products.append(author(sample))
        except ValueError as error:
            rejected.append(dict(id=sample['id'], reason=str(error)))
    common_ms = (time.perf_counter()-began)*1000
    times, sizes, triangles = [], [], []
    for iteration in range(3):
        elapsed, payload, count = 0., 0, 0
        for product in products:
            began = time.perf_counter()
            try:
                geometry = prepare(product)[0] if method == 'bands' else union(product)
                elapsed += time.perf_counter()-began
                if iteration == 0:
                    check_measurements(product, geometry)
            except (ValueError, AssertionError) as error:
                raise AssertionError(f'{site}/{method}/{product["id"]}: {error}') from error
            payload += sum(geometry[field].nbytes for field in ('vertices', 'faces', 'uv', 'materials'))
            count += len(geometry['faces'])
            del geometry
        times.append(elapsed*1000)
        sizes.append(payload)
        triangles.append(count)
    assert len(set(sizes)) == len(set(triangles)) == 1
    peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*(1 if sys.platform == 'darwin' else 1024)
    return dict(site=site, method=method, requested_source_polygons=len(samples), products=len(products),
                bands=sum(len(p['layers']) for p in products),
                setbacks=sum(len(p['layers']) > 1 for p in products),
                planned_features=sum(len(p['features']) for p in products),
                facade_runs=sum(len(p['facades']) for p in products),
                source_sha256=receipt['receipt']['sha256'], rejected=rejected,
                source_geometry_rejected=receipt['rejected'],
                common_decode_and_author_ms=common_ms, generation_ms=times,
                median_generation_ms=statistics.median(times), hull_payload_bytes=sizes[0],
                hull_triangles=triangles[0], process_peak_rss_bytes=peak,
                independently_checked_volume_and_area=len(products))


def run(args):
    args.output.mkdir(parents=True, exist_ok=True)
    rows = []
    for site in args.sites:
        for limit in args.limits:
            pair = []
            for method in ('bands', 'union'):
                command = [sys.executable, str(Path(__file__)), '--worker', method, '--site', site, '--limit', str(limit)]
                result = json.loads(subprocess.check_output(command, text=True, timeout=600))
                pair.append(result)
                rows.append(result)
                print(json.dumps({key: result[key] for key in ('site', 'method', 'products', 'bands', 'setbacks',
                                                              'median_generation_ms', 'hull_payload_bytes',
                                                              'hull_triangles', 'process_peak_rss_bytes')}), flush=True)
            for field in ('products', 'bands', 'setbacks', 'planned_features', 'facade_runs', 'source_sha256', 'rejected'):
                assert pair[0][field] == pair[1][field], field
    report = dict(**revision(), measurements=rows,
                  environment=dict(python=sys.version, numpy=np.__version__, shapely=shapely.__version__,
                                   geos=shapely.geos_version_string, manifold3d=version('manifold3d'),
                                   platform=platform.platform()),
                  scope='Complete exposed coarse bodies from identical compact band/facade plans. Common source decode and authoring separated; no frame, GPU or A18-Pro timing.',
                  memory_scope='Payload: XYZ f64, UV f32, triangles i32, material u8. Selection metadata and separately planned solar/balcony products excluded equally. Peak RSS includes each whole fresh worker and checks.',
                  quality='Independent volume and exterior-area checks for every source body, outside generation timings; analytic courtyard/multipolygon/orbit tests in verify_massing.py.',
                  limitations=['flat-roof nested bands only', 'parts and tile seams not reconciled',
                               'no scene occlusion or native world/device gate'])
    (args.output/'benchmark.json').write_text(json.dumps(report, indent=2)+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/massing-measurements'))
    parser.add_argument('--sites', nargs='+', default=['Vienna', 'Tokyo', 'CentralPark'])
    parser.add_argument('--limits', nargs='+', type=int, default=[16, 128, 0])
    parser.add_argument('--worker', choices=('bands', 'union'))
    parser.add_argument('--site')
    parser.add_argument('--limit', type=int, default=0)
    args = parser.parse_args()
    if args.worker:
        print(json.dumps(worker(args.site, args.worker, args.limit)))
    else:
        run(args)


if __name__ == '__main__':
    main()
