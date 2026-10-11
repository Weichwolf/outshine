"""Independent area, geometry and bounded-work checks for the facade experiment."""

import json
from collections import Counter
from pathlib import Path
import subprocess
import sys
import tempfile

import numpy as np
import shapely
from unittest.mock import patch

from codec import decode, encode
from demand import View, select
from facade_geometry import mesh
from facade_plan import Repeat, REPEAT_DTYPE, coverage, expanded_splits, opening_rectangles, pack_repeats, plan
from offline_probe import probe


def coverage_reference(rectangles, centres, footprint):
    lower, upper = centres-footprint*.5, centres+footprint*.5
    sides = np.maximum(0., np.minimum(upper[:, None], rectangles[None, :, 2:])
                       -np.maximum(lower[:, None], rectangles[None, :, :2]))
    return np.prod(sides, axis=-1).sum(axis=-1)/np.prod(footprint, axis=-1)


def verify_filter():
    rng = np.random.default_rng(6173)
    checked, largest_error = 0, 0.
    for columns, rows in ((1, 1), (13, 6), (35, 17)):
        for width, height in ((.8, .58), (.5, .72), (0., 0.)):
            repeat = Repeat(47.3, 19.4, columns, rows, width, height)
            rectangles = expanded_splits(repeat)
            np.testing.assert_array_equal(opening_rectangles(repeat), rectangles)
            centres = rng.uniform([-20, -20], [70, 40], (512, 2))
            footprints = rng.uniform(.001, 80, (512, 2))
            actual, expected = coverage(repeat, centres, footprints), coverage_reference(rectangles, centres, footprints)
            error = float(np.max(np.abs(actual-expected)))
            assert error < 1e-11, error
            largest_error = max(largest_error, error)
            checked += len(centres)
            mean = coverage(repeat, [repeat.width_m*.5, repeat.height_m*.5], [repeat.width_m, repeat.height_m])
            assert abs(mean-width*height) < 1e-12
    huge = Repeat(1e6, 1e6, 1_000_000, 1_000_000, .8, .58)
    selected = opening_rectangles(huge, [32.1, 19.1, 33.9, 20.9])
    assert len(selected) == 4 and huge.count == 1_000_000_000_000
    assert REPEAT_DTYPE.itemsize == 48 and [REPEAT_DTYPE.fields[n][1] for n in REPEAT_DTYPE.names] == [0, 8, 16, 24, 28, 32, 40]
    assert pack_repeats([dict(repeat=huge)])['columns'][0] == 1_000_000
    return dict(reference_boxes=checked, maximum_coverage_error=largest_error,
                whole_facade_mean_preserved=True, bounded_selection_from_trillion_openings=len(selected))


def verify_closed_shell(product):
    shell = product['shell']
    directed, undirected = Counter(), Counter()
    for triangle in shell['vertices'][shell['faces']]:
        for a, b in zip(triangle, np.roll(triangle, -1, axis=0)):
            a, b = tuple(a), tuple(b)
            directed[(a, b)] += 1
            undirected[tuple(sorted((a, b)))] += 1
    assert set(undirected.values()) == {2}
    assert all(count == directed[(b, a)] for (a, b), count in directed.items())


def verify_geometry():
    polygons = [shapely.box(0, 0, 20, 12),
                shapely.Polygon([(0, 0), (20, 0), (20, 5), (7, 5), (7, 15), (0, 15)]),
                shapely.Polygon([(0, 0), (20, 0), (20, 20), (0, 20)], [[(5, 5), (5, 15), (15, 15), (15, 5)]])]
    triangles = 0
    for polygon in polygons:
        sample = dict(id='geometry-case', polygon=polygon, properties={'render_height': 12., 'render_min_height': -2.})
        for style in ('bauhaus', 'artdeco'):
            product = plan(sample, style)
            verify_closed_shell(product)
            reversed_sample = dict(sample, polygon=shapely.reverse(polygon))
            assert plan(reversed_sample, style)['seed'] == product['seed']
            translated = dict(sample, polygon=shapely.transform(polygon, lambda xy: xy+[1e4, -5e4]))
            assert plan(translated)['seed'] == product['seed']
            demand = select(product, View(np.array([-50., -60., 38.]), np.array([10., 10., 5.])))
            assert len(demand['faces']) < len(product['facades']) and demand['bottom'].is_empty
            for fine in (False, True):
                geometry = mesh(product, demand, detail=fine)
                vertices, faces = geometry['vertices'], geometry['faces']
                points = vertices[faces]
                normals = np.cross(points[:, 1]-points[:, 0], points[:, 2]-points[:, 0])
                assert np.isfinite(vertices).all() and np.isfinite(geometry['uv']).all()
                assert np.all(np.linalg.norm(normals, axis=1) > 1e-12)
                np.testing.assert_allclose([vertices[:, 2].min(), vertices[:, 2].max()], [-2., 12.], atol=1e-12)
                roof = np.all(points[:, :, 2] == 12., axis=1)
                assert abs(normals[roof, 2].sum()*.5-polygon.area) < 1e-8
                assert np.all(normals[roof, 2] > 0)
                for triangle in points[roof]:
                    assert shapely.covers(polygon, shapely.Polygon(triangle[:, :2]))
                triangles += len(faces)
            bounded = mesh(product, demand, max_openings=1)
            assert bounded['detail_openings'] <= 1 and bounded['detail_fallbacks']
            assert np.array_equal(bounded['vertices'], mesh(product, demand, detail=False)['vertices'])
    thin = plan(dict(id='thin', polygon=shapely.box(0, 0, 30, .1), properties={'building': 'apartments', 'render_height': 9.}))
    thin_demand = select(thin, View(np.array([15., -20., 8.]), np.array([15., 0., 4.])))
    assert any(f['reason'] == 'recess_crosses_footprint_boundary' for f in mesh(thin, thin_demand)['detail_fallbacks'])
    shaft = plan(dict(id='silo', polygon=shapely.box(0, 0, 10, 10), properties={'building': 'silo', 'render_height': 25.}))
    assert sum(f['repeat'].count for f in shaft['facades']) == 0
    colour = plan(dict(id='colour', polygon=polygons[0], properties={'building:colour': '#ff0000', 'colour': '#0000ff'}))
    np.testing.assert_array_equal(colour['wall_linear'], [1., 0., 0.])
    supplied = plan(dict(id='height-precedence', polygon=polygons[0], properties={'height': 12., 'render_height': 7.,
                                                                              'min_height': -2., 'render_min_height': 0.}))
    assert supplied['top_m'] == 12. and supplied['bottom_m'] == -2.
    assert supplied['provenance']['bottom'] == 'supplied'
    return dict(cases=len(polygons)*2, validated_triangles=triangles, courtyards_preserved=True,
                negative_base_preserved=True, budget_keeps_complete_shell=True, thin_wall_recess_rejected=True,
                source_colour_precedence=True, source_height_precedence=True,
                supplied_silo_closed=True, prepared_hulls_closed=True, camera_rebase_seed_stable=True)


def verify():
    return dict(filter=verify_filter(), geometry=verify_geometry(), demand_and_cache=verify_demand_and_cache())


def verify_demand_and_cache():
    product = plan(dict(id='motion', polygon=shapely.box(-10, -10, 10, 10),
                        properties={'render_height': 15., 'building': 'apartments', 'custom': 'preserved'}))
    encoded = encode(product)
    edges = set()
    with patch('facade_plan.plan', side_effect=AssertionError('hit must not regenerate plans')), \
         patch('shapely.constrained_delaunay_triangles', side_effect=AssertionError('hit must not rebuild shell')):
        loaded = decode(encoded)
        assert encode(loaded) == encoded and loaded['source_properties']['custom'] == 'preserved'
        for angle in np.linspace(0., 2*np.pi, 16, endpoint=False):
            view = View(np.array([50*np.cos(angle), 50*np.sin(angle), 25.]), np.array([0., 0., 7.]))
            request = select(loaded, view)
            edges.update(f['edge'] for f in request['faces'])
            assert len(request['faces']) <= 2 and request['bottom'].is_empty
            first, second = mesh(product, request), mesh(loaded, request)
            for key in ('vertices', 'faces', 'uv', 'materials'):
                np.testing.assert_array_equal(first[key], second[key])
        far = select(loaded, View(np.array([3000., -3000., 1000.]), np.array([0., 0., 7.])))
        assert not any(f['detail'] for f in far['faces'])
        with patch('facade_geometry.opening_rectangles', side_effect=AssertionError('far demand must not expand openings')):
            distant = mesh(loaded, far)
        assert distant['detail_openings'] == 0 and len(distant['faces']) <= 8
        away = select(loaded, View(np.array([50., 50., 25.]), np.array([100., 100., 25.])))
        assert len(mesh(loaded, away)['faces']) == 0
    assert len(edges) == 4
    records = [json.loads(encoded) for _ in range(5)]
    del records[0]['product']['facades'][0]
    records[1]['product']['top_m'] = float('nan')
    records[2]['product']['facades'][0]['normal'] = [0., 0., 1.]
    records[3]['product']['facades'][0]['repeat']['width_m'] *= 2
    del records[4]['product']['shell']
    for record in records:
        try:
            decode(json.dumps(record))
        except ValueError:
            pass
        else:
            raise AssertionError('partial or inconsistent cache record accepted')
    with tempfile.TemporaryDirectory(prefix='outshine-tests.outshine_buildings.') as directory:
        path = Path(directory)/'enriched-plan.json'
        path.write_bytes(encoded)
        actual = json.loads(subprocess.check_output([sys.executable, str(Path(__file__).with_name('offline_probe.py')),
                                                    str(path)], text=True, timeout=30))
        assert actual == probe(encoded)
    return dict(camera_independent_cache_bytes=len(encoded), hit_repeats_no_plan_generation=True,
                hit_repeats_no_shell_triangulation=True, hit_detail_matches_miss=True,
                orbit_views=16, orbit_covers_all_edges=True,
                far_triangles=len(distant['faces']), far_expands_no_openings=True, away_emits_nothing=True,
                inconsistent_records_rejected=len(records), fresh_offline_process_matches=True)


if __name__ == '__main__':
    import json
    print(json.dumps(verify(), indent=2))
