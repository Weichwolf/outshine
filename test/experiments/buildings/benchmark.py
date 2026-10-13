"""Fresh-process encoding comparison on identical pinned real facade plans, not frame timings."""

import argparse
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

from facade_plan import expanded_splits, opening_rectangles, pack_repeats, plan
from evidence import revision
from samples import read_site
from verify import verify


def worker(site, method, limit):
    began = time.perf_counter()
    samples, receipt = read_site(site)
    samples = sorted(samples, key=lambda s: s['id'])
    if limit:
        samples = samples[:limit]
    products, rejected = [], []
    for sample in samples:
        try:
            products.append(plan(sample))
        except ValueError as error:
            rejected.append(dict(id=sample['id'], reason=str(error)))
    facades = [facade for product in products for facade in product['facades']]
    common_ms = (time.perf_counter()-began)*1000
    windows = sum(f['repeat'].count for f in facades)
    times, payload = [], 0
    for _ in range(3):
        began = time.perf_counter()
        if method == 'compact':
            result = pack_repeats(facades)
        else:
            expand = expanded_splits if method == 'expanded' else opening_rectangles
            result = [expand(f['repeat']) for f in facades]
        times.append((time.perf_counter()-began)*1000)
        payload = result.nbytes if method == 'compact' else sum(a.nbytes for a in result)
        if method != 'compact':
            assert sum(len(a) for a in result) == windows
        else:
            assert len(result) == len(facades)
        del result
    peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*(1 if sys.platform == 'darwin' else 1024)
    return dict(site=site, method=method, requested_source_polygons=len(samples), products=len(products),
                facade_runs=len(facades), potential_openings=windows, rejected=rejected,
                source_geometry_rejected=receipt['rejected'], source_sha256=receipt['receipt']['sha256'],
                common_decode_and_plan_ms=common_ms, generation_ms=times,
                median_generation_ms=statistics.median(times), repeat_payload_bytes=payload,
                process_peak_rss_bytes=peak)


def run(args):
    args.output.mkdir(parents=True, exist_ok=True)
    rows = []
    for site in args.sites:
        for limit in (16, 128, 0):
            pair = []
            for method in ('expanded', 'vectorized', 'compact'):
                command = [sys.executable, str(Path(__file__)), '--worker', method, '--site', site, '--limit', str(limit)]
                result = json.loads(subprocess.check_output(command, text=True, timeout=180))
                pair.append(result)
                rows.append(result)
                print(json.dumps({k: result[k] for k in ('site', 'method', 'products', 'potential_openings',
                                                        'median_generation_ms', 'repeat_payload_bytes', 'process_peak_rss_bytes')}), flush=True)
            for field in ('products', 'facade_runs', 'potential_openings', 'source_sha256', 'rejected'):
                assert pair[0][field] == pair[1][field] == pair[2][field]
    report = dict(**revision(), verification=verify(), measurements=rows,
                  environment=dict(python=sys.version, numpy=np.__version__, shapely=shapely.__version__,
                                   geos=shapely.geos_version_string, platform=platform.platform()),
                  scope='Equivalent complete opening plans, common footprint/height/material preparation separated; no mesh, frame, GPU or A18-Pro timing.',
                  memory_scope='Payload is repeat encoding only; peak RSS includes fresh-process libraries, source decoding, common plans and emission. Shared geometry is excluded from both payload counts.',
                  limits=['flat-roof pilot', 'no native asset cache or world render gate',
                          'source tile fragments/overlapping parts not yet reconciled',
                          'camera demand currently uses facing/frustum/movement, without scene occlusion',
                          'world budgets and GPU/pass timings remain unproven'])
    (args.output/'benchmark.json').write_text(json.dumps(report, indent=2)+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/measurements'))
    parser.add_argument('--sites', nargs='+', default=['Vienna', 'Tokyo', 'CentralPark'])
    parser.add_argument('--worker', choices=('expanded', 'vectorized', 'compact'))
    parser.add_argument('--site')
    parser.add_argument('--limit', type=int, default=0)
    args = parser.parse_args()
    if args.worker:
        print(json.dumps(worker(args.site, args.worker, args.limit)))
    else:
        run(args)


if __name__ == '__main__':
    main()
