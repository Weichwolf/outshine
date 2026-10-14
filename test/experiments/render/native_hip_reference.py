"""Compare native roof plans with pinned real-world Python outputs, without regeneration."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shlex
import subprocess
import time

import shapely
from shapely.geometry.polygon import orient

ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / 'test/experiments/buildings/reference/hip-plans.json'
SOURCE = ROOT / 'test/scripts/hip-roof-reference.cpp'


def compile_helper(output):
    records = json.loads((ROOT / 'compile_commands.json').read_text())
    record = next(row for row in records
                  if Path(row['file']).resolve() == ROOT / 'src/generators/building/BuildingDraw.cpp')
    flags = record.get('arguments') or shlex.split(record['command'])
    command, skip = [], False
    for flag in flags:
        if skip:
            skip = False
        elif flag == '-o':
            skip = True
        elif flag != '-c' and Path(flag) != Path(record['file']):
            command.append(flag)
    environment = os.environ.copy()
    environment['PKG_CONFIG_PATH'] = str(ROOT / 'build/deps/install/lib/pkgconfig') + os.pathsep + environment.get('PKG_CONFIG_PATH', '')
    libraries = shlex.split(subprocess.check_output(
        ['pkg-config', '--libs', 'sfcgal', 'geos'], env=environment, text=True, timeout=30))
    binary = output / 'hip-roof-reference'
    command += [str(SOURCE), str(ROOT / 'build/libgenerators.a'),
                str(ROOT / 'build/liboutshine.a'), *libraries, '-o', str(binary)]
    subprocess.run(command, cwd=ROOT, check=True, timeout=120)
    return binary, command


def input_text(row):
    polygon = orient(shapely.Polygon(row['outer'], row['holes']), sign=1.)
    rings = [list(ring.coords)[:-1] for ring in (polygon.exterior, *polygon.interiors)]
    words = [str(len(rings))]
    for ring in rings:
        words.append(str(len(ring)))
        words.extend(f'{x:.17g} {y:.17g}' for x, y in ring)
    origin = row['frame']['origin_xy_m']
    queries = row['expected']['positions_local_xy_m']
    words.append(str(len(queries)))
    words.extend(f'{x+origin[0]:.17g} {y+origin[1]:.17g}' for x, y in queries)
    return '\n'.join(words) + '\n'


def read_plan(text):
    lines = iter(text.splitlines())
    head = next(lines).split()
    if head[0] != 'plan':
        raise ValueError(text[:200])
    origin = list(map(float, head[1:3]))
    maximum = float(head[3])
    facets = {}
    for _ in range(int(head[5])):
        head = next(lines).split()
        if head[0] != 'facet':
            raise ValueError('missing facet')
        owner = tuple(map(int, head[1:3]))
        if owner in facets:
            raise ValueError('duplicate source-edge ownership')
        points = [list(map(float, next(lines).split())) for _ in range(int(head[6]))]
        facets[owner] = dict(points=points, gradient=list(map(float, head[3:5])),
                             intercept=float(head[5]))
    heights = []
    for line in lines:
        head = line.split()
        heights.append(float(head[1]) if head[0] == 'height' else None)
    return origin, maximum, facets, heights


def compare(row, text, tolerance):
    origin, maximum, facets, heights = read_plan(text)
    expected = row['expected']
    reference_origin = row['frame']['origin_xy_m']
    if origin != reference_origin:
        raise ValueError('different metric origin')
    points = expected['positions_local_xy_m']
    distances = expected['wavefront_distances_m']
    if len(heights) != len(distances) or any(x is None for x in heights):
        raise ValueError('reference roof point missing from native height field')
    height_error = max(abs(a-b) for a, b in zip(heights, distances))
    area_error = plane_error = overlap = 0.
    reference_facets = {tuple(f['source_edge']): f for f in expected['facets']}
    missing = sorted(reference_facets.keys()-facets.keys())
    unexpected = sorted(facets.keys()-reference_facets.keys())
    polygons = []
    for owner, native in facets.items():
        actual = shapely.Polygon(native['points'])
        if not actual.is_valid or actual.area <= 0:
            raise ValueError('invalid native facet')
        polygons.append(actual)
        if owner not in reference_facets:
            continue
        reference = reference_facets[owner]
        stored = shapely.Polygon([points[i] for i in reference['nodes']])
        area_error = max(area_error, actual.symmetric_difference(stored).area)
        for x, y in [*native['points'], *(points[i] for i in reference['nodes'])]:
            a = x*native['gradient'][0] + y*native['gradient'][1] + native['intercept']
            b = x*reference['gradient'][0] + y*reference['gradient'][1] + reference['intercept_m']
            plane_error = max(plane_error, abs(a-b))
    coverage = shapely.union_all(polygons)
    source = shapely.Polygon([(x-origin[0], y-origin[1]) for x, y in row['outer']],
                             [[(x-origin[0], y-origin[1]) for x, y in ring] for ring in row['holes']])
    coverage_error = coverage.symmetric_difference(source).area
    overlap = max(0., sum(p.area for p in polygons)-coverage.area)
    area_limit = max(1e-7, source.length*tolerance['position_tolerance_m'])
    peak_error = abs(maximum-max(distances))
    mode_error = 0.
    for case in expected['cases']:
        factor = case['value']/maximum if case['mode'] == 'byHeight' else math.tan(math.radians(case['value']))
        mode_error = max(mode_error, *(abs(a*factor-b*case['height_factor'])
                                      for a, b in zip(heights, distances)))
    passed = (not missing and not unexpected
              and area_error <= area_limit and coverage_error <= area_limit and overlap <= 1e-7
              and plane_error <= tolerance['wavefront_plane_tolerance_m']
              and height_error <= tolerance['height_tolerance_m']
              and mode_error <= tolerance['height_tolerance_m'])
    return dict(passed=passed, facets=len(facets), nodes=len(heights),
                missing_source_edges=missing, unexpected_source_edges=unexpected,
                facet_area_error_m2=area_error, coverage_error_m2=coverage_error,
                overlap_m2=overlap, area_limit_m2=area_limit, plane_error_m=plane_error,
                wavefront_height_error_m=height_error, peak_distance_error_m=peak_error,
                height_modes_error_m=mode_error)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', type=Path, default=FIXTURE)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/experiments/render/native-hip-reference')
    parser.add_argument('--timeout', type=float, default=2.)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error('--timeout must be positive')
    args.output.mkdir(parents=True, exist_ok=True)
    binary, command = compile_helper(args.output)
    data = json.loads(args.fixture.read_text())
    if data['schema'] != 'outshine.hip-plan-reference/1':
        raise ValueError('unknown fixture contract')
    if len(data['records']) != 48 or len({row['id'] for row in data['records']}) != 48:
        raise ValueError('fixture must contain 48 distinct real-world roofs')
    results = []
    for row in data['records']:
        start = time.monotonic()
        try:
            result = subprocess.run([str(binary)], input=input_text(row), text=True,
                                    capture_output=True, timeout=args.timeout, check=True)
            checks = compare(row, result.stdout, data['comparison'])
        except subprocess.CalledProcessError as error:
            checks = dict(passed=False, error=str(error), native_output=error.stdout[:200],
                          native_error=error.stderr[:500])
        except (ValueError, subprocess.SubprocessError) as error:
            checks = dict(passed=False, error=str(error))
        results.append(dict(id=row['id'], elapsed_ms=(time.monotonic()-start)*1000, **checks))
    report = dict(commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  implementation_tree_dirty=bool(subprocess.check_output(
                      ['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip()),
                  source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                  runner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  native_sha256={str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                 for path in (ROOT / 'build/libgenerators.a', ROOT / 'build/liboutshine.a',
                                              ROOT / 'src/generators/building/HipRoofPlan.cpp')},
                  fixture_sha256=hashlib.sha256(args.fixture.read_bytes()).hexdigest(),
                  compile=command, timeout_seconds=args.timeout,
                  scope='native standard roof coverage/planes/height; external per-case timeout, no engine deadline proof',
                  results=results)
    (args.output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    failed = [row['id'] for row in results if not row['passed']]
    print(f'{len(results)-len(failed)}/{len(results)} native roof comparisons passed')
    if failed:
        print('Failed:', ', '.join(failed))
    raise SystemExit(bool(failed))


if __name__ == '__main__':
    main()
