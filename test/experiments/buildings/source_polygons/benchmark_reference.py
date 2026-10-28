import argparse
import collections
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import statistics
import time
import tracemalloc

from canonical_polygons import components, load_exports, resolve
from check_reference import fixture_paths, run_checks, semantic

ROOT = Path(__file__).parent


def resolve_chunk(job):
    method, indexed = job
    began = time.process_time()
    result = [(index, semantic(resolve(group, method))) for index, group in indexed]
    return result, time.process_time() - began


def digest(result):
    return hashlib.sha256(json.dumps(result, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description='Compare equal-output source-polygon procedures.')
    parser.add_argument('--output', required=True, type=Path, help='Explicit private JSON result path.')
    args = parser.parse_args()
    source_hashes = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                     for name in ['canonical_polygons.py', 'check_reference.py', 'benchmark_reference.py']}
    controls = run_checks()
    paths = fixture_paths()
    began = time.perf_counter()
    observations, residual = load_exports(paths)
    load_seconds = time.perf_counter() - began
    began = time.perf_counter()
    groups = components(observations)
    component_seconds = time.perf_counter() - began
    indexed = list(enumerate(groups))
    baseline, _ = resolve_chunk(('whole-first', indexed))
    expected = digest(baseline)
    results = {'scope': 'Python source-normalization candidates on the two preserved tiles. No native/frame/device readiness claim; unknown cases remain unresolved.',
               'source_profile': 'observed extent4096 and buffer64 grid units; not a universal provider declaration',
               'source_hashes': source_hashes,
               'input_hashes': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
               'controls': controls['count'], 'observations': len(observations), 'components': len(groups),
               'max_grid_inverse_residual': residual, 'load_seconds': load_seconds,
               'component_seconds': component_seconds, 'canonical_result_sha256': expected,
               'statuses': dict(collections.Counter(r['status'] for _, r in baseline)), 'runs': []}
    for method in ['whole-first', 'boundary-first', 'boundary-first', 'whole-first'] * 2:
        wall, cpu = time.perf_counter(), time.process_time()
        actual, _ = resolve_chunk((method, indexed))
        row = {'mode': 'serial', 'method': method, 'wall_seconds': time.perf_counter() - wall,
               'cpu_seconds': time.process_time() - cpu, 'result_sha256': digest(actual)}
        assert row['result_sha256'] == expected
        results['runs'].append(row)
        print('serial', method, row['wall_seconds'], flush=True)
    logical = os.cpu_count() or 1
    available = len(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else logical
    workers = min(available, len(groups))
    results['logical_cpus'], results['available_cpus'], results['workers'] = logical, available, workers
    chunks = [indexed[i::workers] for i in range(workers)]
    with concurrent.futures.ProcessPoolExecutor(max_workers=workers) as pool:
        for iteration, method in enumerate(['whole-first', 'boundary-first', 'boundary-first', 'whole-first']):
            began = time.perf_counter()
            pieces = list(pool.map(resolve_chunk, [(method, chunk) for chunk in chunks]))
            actual = sorted([row for piece, _ in pieces for row in piece], key=lambda x: x[0])
            row = {'mode': 'processes', 'method': method, 'iteration': iteration,
                   'includes_cold_worker_start': iteration == 0,
                   'wall_seconds': time.perf_counter() - began,
                   'worker_cpu_seconds': sum(cpu for _, cpu in pieces), 'result_sha256': digest(actual)}
            assert row['result_sha256'] == expected
            results['runs'].append(row)
            print('processes', method, row['wall_seconds'], flush=True)
    results['python_heap'] = []
    for method in ['whole-first', 'boundary-first']:
        tracemalloc.start()
        actual, _ = resolve_chunk((method, indexed))
        _, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        assert digest(actual) == expected
        results['python_heap'].append({'method': method, 'peak_traced_python_bytes_excludes_geos': peak})
        del actual
    results['serial_medians'] = {method: {
        'wall_seconds': statistics.median(r['wall_seconds'] for r in results['runs'] if r['mode'] == 'serial' and r['method'] == method),
        'cpu_seconds': statistics.median(r['cpu_seconds'] for r in results['runs'] if r['mode'] == 'serial' and r['method'] == method)
    } for method in ['whole-first', 'boundary-first']}
    assert source_hashes == {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in source_hashes}
    args.output.write_text(json.dumps(results, indent=2) + '\n')
    print(json.dumps({'medians': results['serial_medians'], 'workers': workers, 'statuses': results['statuses']}), flush=True)


if __name__ == '__main__':
    main()
