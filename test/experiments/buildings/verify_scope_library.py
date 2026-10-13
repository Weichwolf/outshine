"""Compare installed scope transforms to their committed experiment on real meshes."""

import argparse
from dataclasses import replace
import gc
import hashlib
import json
from pathlib import Path
import resource
import subprocess
import sys
import time
import tracemalloc
from types import ModuleType

import numpy as np

from evidence import revision
from modelling_scope_probe import cga_mesh
from outshine_ref.buildings.modelling.shape import Shape
from outshine_ref.buildings.modelling import scope as library


def baseline(commit):
    raw = subprocess.check_output(['git', 'show', commit+':test/experiments/buildings/modelling_scope.py'])
    module = ModuleType('baseline_building_scope')
    sys.modules[module.__name__] = module
    exec(compile(raw, '<committed-building-scope>', 'exec'), module.__dict__)
    return module, hashlib.sha256(raw).hexdigest()


def emit(shape, module):
    source = shape.scope
    frame = lambda item: module.Frame(item.position, item.orientation)
    scope = module.Scope(source.position, source.orientation, source.size, frame(source.pivot), frame(source.object_frame))
    scope = scope.r(12., 25., -9.)
    scope = scope.t(module.Relative(.2), 2., module.Relative(-.1))
    scope = scope.s(module.Relative(1.2), module.Relative(.8), module.Relative(.9))
    scope = scope.r(-5., 11., 7., 'scopeCenter')
    scope = scope.translate('rel', 'world', 3., -2., 4.)
    return replace(shape, scope=scope).emit()


def costs(calls):
    times = {name: [] for name in calls}
    for call in calls.values():
        call()
    order = tuple(calls)
    for index in range(21):
        for name in (order if index % 2 == 0 else order[::-1]):
            start = time.perf_counter_ns()
            calls[name]()
            times[name].append((time.perf_counter_ns()-start)/1e6)
    result = {}
    for name, call in calls.items():
        gc.collect()
        tracemalloc.start()
        call()
        _, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        result[name] = dict(p50_ms=float(np.percentile(times[name], 50)), p95_ms=float(np.percentile(times[name], 95)),
                            python_tracked_peak_bytes=peak, samples=21)
    return result


def compare(old):
    root = Path('build/experiments/buildings')
    paths = [root/'opening-inspections'/('opening-'+label+'-mesh.npz')
             for label in ('rectangle', 'vienna-compact', 'tokyo-compact')]
    paths.append(root/'cities-close/tokyo-city-mesh.npz')
    rows = []
    for path in paths:
        with np.load(path) as archive:
            mesh = cga_mesh(dict(archive))
        shape = Shape.from_mesh(mesh)
        before, after = emit(shape, old), emit(shape, library)
        if set(before) != set(after):
            raise AssertionError('Scope migration changed emitted streams')
        for key in before:
            np.testing.assert_array_equal(after[key], before[key], err_msg=path.name+' '+key)
        rows.append(dict(input=str(path), input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                         vertices=len(mesh['vertices']), triangles=len(mesh['faces']),
                         emitted_streams=sorted(after), equality='bit-identical',
                         **costs(dict(baseline=lambda: emit(shape, old), installed=lambda: emit(shape, library)))))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-commit', default='4ab0ce7c7')
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/scope-library-comparison.json'))
    args = parser.parse_args()
    old, digest = baseline(args.baseline_commit)
    rows = compare(old)
    result = revision() | dict(status='pass', baseline_commit=args.baseline_commit, baseline_source_sha256=digest,
                              measurements=rows, process_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*1024,
                              measurement_scope='Linux Python process; five transforms and full mesh emission; warm paired p50/p95 with alternating order. '
                              'Tracemalloc is tracked allocation per call, RSS is whole-process peak including inputs; '
                              'no GPU, native-frame or A18 claim.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(dict(status='pass', bit_identical_meshes=len(rows))))


if __name__ == '__main__':
    main()
